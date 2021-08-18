// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice} from './fake_bluetooth_config.m.js';
// clang-format on

suite('OsPairedBluetoothListItemTest', function() {
  /** @type {!SettingsPairedBluetoothListItemElement|undefined} */
  let pairedBluetoothListItem;

  setup(function() {
    pairedBluetoothListItem =
        document.createElement('os-settings-paired-bluetooth-list-item');
    document.body.appendChild(pairedBluetoothListItem);
    Polymer.dom.flush();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {number} batteryPercentage
   */
  async function setBatteryPercentage(batteryPercentage) {
    pairedBluetoothListItem.device.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage}
    };
    pairedBluetoothListItem.device = {...pairedBluetoothListItem.device};
    return flushAsync();
  }

  /**
   * @param {boolean} isLowBattery
   * @param {boolean} batteryIconRange
   */
  function assertBatteryUIState(isLowBattery, batteryIconRange) {
    assertEquals(pairedBluetoothListItem.isLowBattery_, isLowBattery);
    assertEquals(
        pairedBluetoothListItem.shadowRoot.querySelector('#batteryIcon').icon,
        'os-settings:battery-' + batteryIconRange);
  }

  test(
      'Device name, type, battery percentage and a11y labels',
      async function() {
        // Device with no nickname, battery info and unknown device type.
        const publicName = 'BeatsX';
        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456789', /*publicName=*/ publicName,
            /*connected=*/ true);
        pairedBluetoothListItem.device = device;

        const itemIndex = 3;
        const listSize = 15;
        pairedBluetoothListItem.itemIndex = itemIndex;
        pairedBluetoothListItem.listSize = listSize;
        await flushAsync();

        const getDeviceName = () => {
          return pairedBluetoothListItem.$.deviceName;
        };
        const getBatteryPercentage = () => {
          return pairedBluetoothListItem.shadowRoot.querySelector(
              '#batteryPercentage');
        };
        const getDeviceTypeIcon = () => {
          return pairedBluetoothListItem.$.deviceTypeIcon;
        };
        const getItemA11yLabel = () => {
          return pairedBluetoothListItem.shadowRoot.querySelector('.list-item')
              .ariaLabel;
        };
        const getSubpageButtonA11yLabel = () => {
          return pairedBluetoothListItem.$.subpageButton.ariaLabel;
        };
        assertTrue(!!getDeviceName());
        assertEquals(getDeviceName().innerText, publicName);
        assertFalse(!!getBatteryPercentage());
        assertTrue(!!getDeviceTypeIcon());
        assertEquals(getDeviceTypeIcon().icon, 'os-settings:bluetooth');
        assertEquals(
            getItemA11yLabel(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemA11yLabelTypeUnknown', itemIndex + 1,
                listSize, publicName));
        assertEquals(
            getSubpageButtonA11yLabel(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemSubpageButtonA11yLabel', publicName));

        // Set device nickname, type and battery info.
        const nickname = 'nickname';
        device.nickname = nickname;
        device.deviceProperties.deviceType =
            chromeos.bluetoothConfig.mojom.DeviceType.kComputer;
        const batteryPercentage = 60;
        device.deviceProperties.batteryInfo = {
          defaultProperties: {batteryPercentage: batteryPercentage}
        };
        pairedBluetoothListItem.device = {...device};
        await flushAsync();

        assertTrue(!!getDeviceName());
        assertEquals(getDeviceName().innerText, nickname);
        assertTrue(!!getBatteryPercentage());
        assertEquals(
            getBatteryPercentage().innerText.trim(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemBatteryPercentage',
                batteryPercentage));
        assertEquals(
            getDeviceTypeIcon().icon, 'os-settings:bluetooth-computer');
        assertEquals(
            getItemA11yLabel(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemA11yLabelTypeComputerWithBatteryInfo',
                itemIndex + 1, listSize, nickname, batteryPercentage));
        assertEquals(
            getSubpageButtonA11yLabel(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemSubpageButtonA11yLabel', nickname));
      });

  test('Battery icon and color', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX', /*connected=*/ true);
    pairedBluetoothListItem.device = device;

    const getBatteryContainer = () => {
      return pairedBluetoothListItem.shadowRoot.querySelector(
          '#batteryContainer');
    };

    // Battery percentage out of bounds, should not be visible.
    await setBatteryPercentage(-10);
    assertFalse(!!getBatteryContainer());

    // Lower bound edge case.
    await setBatteryPercentage(0);
    assertBatteryUIState(/*isLowBattery=*/ true, /*batteryIconRange=*/ '0-7');

    await setBatteryPercentage(3);
    assertBatteryUIState(
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '0-7');

    // Maximum 'low battery' percentage.
    await setBatteryPercentage(24);
    assertBatteryUIState(
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '22-28');

    // Minimum non-'low battery' percentage.
    await setBatteryPercentage(25);
    assertBatteryUIState(
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '22-28');

    await setBatteryPercentage(94);
    assertBatteryUIState(
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Upper bound edge case.
    await setBatteryPercentage(100);
    assertBatteryUIState(
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Battery percentage out of bounds, should not be visible.
    await setBatteryPercentage(101);
    assertFalse(!!getBatteryContainer());
  });
});