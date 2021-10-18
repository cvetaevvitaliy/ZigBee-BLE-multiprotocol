/**
* @brief This is part of the project 'Light Switch'
* @file 'ble.c' 
* Copyright (c) Vitaliy Nimych <vitaliy.nimych@gmail.com>
* Created 06.10.2021
* License-Identifier: ???
**/
#include <zephyr/types.h>
#include <stddef.h>
#include <inttypes.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/bas_client.h>
#include <dk_buttons_and_leds.h>

#include <settings/settings.h>

/**
 * Button to read the battery value
 */
#define KEY_READVAL_MASK DK_BTN1_MSK

#define BAS_READ_VALUE_INTERVAL (10 * MSEC_PER_SEC)

#define BLE_LOG "[ BLE ] "

static struct bt_conn *default_conn;
extern void zigbee_send(void);

static void scan_filter_match(struct bt_scan_device_info *device_info,
                              struct bt_scan_filter_match *filter_match,
                              bool connectable)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
    if (device_info->recv_info->rssi > -52) {
        printk(BLE_LOG "Filters matched. Address: %s RSSI: '%d'\n", addr, device_info->recv_info->rssi);
        
        zigbee_send();
    } else {
        printk(BLE_LOG "Address: %s LOW RSSI: '%d'\n", addr, device_info->recv_info->rssi);
    }
    
}

static void scan_filter_no_match(struct bt_scan_device_info *device_info,
                                 bool connectable)
{
    int err;
    struct bt_conn *conn;
    char addr[BT_ADDR_LE_STR_LEN];
#if 0
    if (device_info->recv_info->rssi > -30) {
        bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
        printk("Found %s RSSI; '%d'\n", addr, device_info->recv_info->rssi);
    }
#endif
    
    if (device_info->recv_info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        bt_addr_le_to_str(device_info->recv_info->addr, addr,
                          sizeof(addr));
        printk("Direct advertising received from %s\n", addr);
        bt_scan_stop();
        
        err = bt_conn_le_create(device_info->recv_info->addr,
                                BT_CONN_LE_CREATE_CONN,
                                device_info->conn_param, &conn);
        
        if (!err) {
            default_conn = bt_conn_ref(conn);
            bt_conn_unref(conn);
        }
    }
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match,
                NULL, NULL);


#define BT_UUID_THINGY_VAL \
    BT_UUID_128_ENCODE(0x97BD89C7, 0xE74E, 0x7C53, 0x98B2, 0x4EAECAF43CA7)

#define BT_UUID_THINGY \
	BT_UUID_DECLARE_128(BT_UUID_THINGY_VAL)
static void scan_init(void)
{
    int err;
    
    struct bt_le_scan_param scan_param = {
            .type = BT_SCAN_TYPE_SCAN_ACTIVE,
            .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
            .interval = 0x0010,
            .window = 0x0010,
    };
    
    struct bt_scan_init_param scan_init = {
            .connect_if_match = 0,
            .scan_param = NULL,
            .conn_param = BT_LE_CONN_PARAM_DEFAULT
    };
    
    bt_scan_init(&scan_init);
    bt_scan_cb_register(&scan_cb);
    
    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_DECLARE_16(0xFEE0));
    if (err) {
        printk(BLE_LOG "Scanning filters cannot be set (err %d)\n", err);
        
        return;
    }
    
    err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
    if (err) {
        printk(BLE_LOG "Filters cannot be turned on (err %d)\n", err);
    }
}


void ble_init(void)
{
    int err;
    
    printk(BLE_LOG "Starting Bluetooth\n");
    
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    
    printk(BLE_LOG "Bluetooth initialized\n");
    
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }
    
    scan_init();
    
    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        printk(BLE_LOG "Scanning failed to start (err %d)\n", err);
        return;
    }
    
    printk(BLE_LOG "Scanning successfully started\n");
}
