#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define CHARDEVICEDRIVER_MAJOR		0
#define CHARDEVICEDRIVER_MINOR		0

struct at24cxx_dev
{
	struct device *dev_device;
	struct i2c_client *client;
	struct cdev cdev;
};

struct at24cxx_dev *at24cxx_devp = NULL;
static struct class *dev_class = NULL;
static u32 chardevicedriver_major = CHARDEVICEDRIVER_MAJOR;
static u32 chardevicedriver_minor = CHARDEVICEDRIVER_MINOR;



static const struct i2c_device_id at24cxx_id[] = { 
    {"at24cxx", 0 }, //��i2c_driver��֧�ֵ�i2c_client
    {} 
}; 
MODULE_DEVICE_TABLE(i2c,at24cxx_id); 


static ssize_t at24cxx_read(struct file *file, char __user *buf, size_t size, loff_t * offset)
{
	unsigned char address;
	unsigned char data;
	struct i2c_msg msg[2];
	int ret;
	
	/* address = buf[0] 
	 * data    = buf[1]
	 */
	if (size != 1)
		return -EINVAL;
	
	copy_from_user(&address, buf, 1);

	/* ���ݴ�����Ҫ��: Դ,Ŀ��,���� */

	/* ��AT24CXXʱ,Ҫ�Ȱ�Ҫ���Ĵ洢�ռ�ĵ�ַ������ */
	msg[0].addr  = at24cxx_devp->client->addr;  /* Ŀ�� */
	msg[0].buf   = &address;              /* Դ */
	msg[0].len   = 1;                     /* ��ַ=1 byte */
	msg[0].flags = 0;                     /* ��ʾд */

	/* Ȼ������������ */
	msg[1].addr  = at24cxx_devp->client->addr;  /* Դ */
	msg[1].buf   = &data;                 /* Ŀ�� */
	msg[1].len   = 1;                     /* ����=1 byte */
	msg[1].flags = I2C_M_RD;                     /* ��ʾ�� */


	ret = i2c_transfer(at24cxx_devp->client->adapter, msg, 2);
	if (ret == 2)
	{
		copy_to_user(buf, &data, 1);
		return 1;
	}
	else
		return -EIO;
}

static ssize_t at24cxx_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	unsigned char val[2];
	struct i2c_msg msg[1];
	int ret;
	
	/* address = buf[0] 
	 * data    = buf[1]
	 */
	if (size != 2)
		return -EINVAL;
	
	copy_from_user(val, buf, 2);

	/* ���ݴ�����Ҫ��: Դ,Ŀ��,���� */
	msg[0].addr  = at24cxx_devp->client->addr;  /* Ŀ�� */
	msg[0].buf   = val;                   /* Դ */
	msg[0].len   = 2;                     /* ��ַ+����=2 byte */
	msg[0].flags = 0;                     /* ��ʾд */

	ret = i2c_transfer(at24cxx_devp->client->adapter, msg, 1);
	if (ret == 1)
		return 2;
	else
		return -EIO;
}



static struct file_operations at24cxx_fops = {
	.owner	= THIS_MODULE,
	//.open	= chardevicedriver_open,
	.read	= at24cxx_read,
	.write	= at24cxx_write,
	//.ioctl	= chardevicedriver_ioctl,
	//.release= chardevicedriver_release,
};

// add one cdev and create one device file
static int chardevicedriver_cdev_add(struct at24cxx_dev *pcdevp, int index)
{
	int ret = -EFAULT;
	dev_t dev = 0;

	// add cdev object
	dev = MKDEV( chardevicedriver_major, chardevicedriver_minor + index );
	cdev_init( &(pcdevp->cdev), &at24cxx_fops);
	ret = cdev_add( &(pcdevp->cdev), dev, 1 );


	// create device
	pcdevp->dev_device = device_create( dev_class, NULL, dev, NULL, "at24cxx%d", MINOR( dev ) );
	
	
	return 0;
}


static int at24cxx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	dev_t dev = 0;

	// register device number
	if( chardevicedriver_major ) { // static device number
		dev = MKDEV( chardevicedriver_major, chardevicedriver_minor );
		ret = register_chrdev_region( dev, 1, "at24cxx" );
	}
	else { // dynamic device number
		ret = alloc_chrdev_region( &dev, chardevicedriver_minor, 1, "at24cxx" );
		chardevicedriver_major = MAJOR( dev );
	}
	
	at24cxx_devp = kmalloc( sizeof( struct at24cxx_dev ), GFP_KERNEL );

	memset(at24cxx_devp, 0, sizeof(struct at24cxx_dev));
	at24cxx_devp->client = client;
	
	dev_class = class_create( THIS_MODULE, "at24cxx" );

	
	chardevicedriver_cdev_add(at24cxx_devp,0);
	
	return 0;
}

static int __devexit at24cxx_remove(struct i2c_client *client)
{
	device_destroy( dev_class, MKDEV( chardevicedriver_major, 0 ) );

	cdev_del( &( at24cxx_devp->cdev ) );
	
	// destroy device class
	class_destroy( dev_class );
	// free memory for private struct
	kfree( at24cxx_devp );
	at24cxx_devp = NULL;

	// unregister device number
	unregister_chrdev_region( MKDEV( chardevicedriver_major, chardevicedriver_minor ), 1 );
	return 0;
}

/* 1. ����һ��i2c_driver�ṹ�� */
/* 2. ����i2c_driver�ṹ�� */
static struct i2c_driver at24cxx_driver = {
	.driver = {
		.name	= "at24cxx",
		.owner = THIS_MODULE,
	},
	.probe=at24cxx_probe,
	.remove=__devexit_p(at24cxx_remove),
	.id_table=at24cxx_id,
};

static int __init at24cxx_init(void)
{
	i2c_add_driver(&at24cxx_driver);
	return 0;
}

static void __exit at24cxx_exit(void)
{
	i2c_del_driver(&at24cxx_driver);
}

module_init(at24cxx_init);
module_exit(at24cxx_exit);

MODULE_LICENSE("GPL");

