// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {BluetoothDeviceBatteryInfoElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_device_battery_info.js';
import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

// clang-format on

suite('CrComponentsBluetoothDeviceBatteryInfoTest', function() {
  /** @type {?BluetoothDeviceBatteryInfoElement} */
  let bluetoothDeviceBatteryInfo;

  setup(function() {
    bluetoothDeviceBatteryInfo = /**
                                    @type {?BluetoothDeviceBatteryInfoElement}
                                  */
        (document.createElement('bluetooth-device-battery-info'));
    document.body.appendChild(bluetoothDeviceBatteryInfo);
    flush();
  });

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {number} batteryPercentage
   */
  async function setDefaultBatteryPercentage(batteryPercentage) {
    bluetoothDeviceBatteryInfo.device.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage}
    };
    bluetoothDeviceBatteryInfo.device =
        /**
         * @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
         */
        (Object.assign({}, bluetoothDeviceBatteryInfo.device));
    return flushAsync();
  }

  /**
   * @param {number} batteryPercentage
   */
  async function setMultipleBatteryPercentage(batteryPercentage) {
    bluetoothDeviceBatteryInfo.device.batteryInfo = {
      leftBudInfo: {batteryPercentage: batteryPercentage},
      caseInfo: {batteryPercentage: batteryPercentage},
      rightBudInfo: {batteryPercentage: batteryPercentage},
    };
    bluetoothDeviceBatteryInfo.device =
        /**
         * @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
         */
        (Object.assign({}, bluetoothDeviceBatteryInfo.device));
    return flushAsync();
  }

  /**
   * @param {number} batteryPercentage
   * @param {boolean} isLowBattery
   * @param {string} batteryIconRange
   */
  function assertDefaultBatteryUIState(
      batteryPercentage, isLowBattery, batteryIconRange) {
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#defaultBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#leftBudBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#caseBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#rightBudBatteryPercentage'));
    assertEquals(
        bluetoothDeviceBatteryInfo.i18n(
            'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage),
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#defaultBatteryPercentage')
            .innerText.trim());
    assertEquals(
        isLowBattery, bluetoothDeviceBatteryInfo.getIsLowBatteryForTest());
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#defaultBatteryIcon'));
    assertEquals(
        'bluetooth:battery-' + batteryIconRange,
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#defaultBatteryIcon')
            .icon);
  }

  /**
   * @param {number} batteryPercentage
   * @param {boolean} isLowBattery
   * @param {string} batteryIconRange
   */
  function assertMultipleBatteryUIState(
      batteryPercentage, isLowBattery, batteryIconRange) {
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#defaultBatteryPercentage'));
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#leftBudBatteryPercentage'));
    assertEquals(
        bluetoothDeviceBatteryInfo.i18n(
            'bluetoothPairedDeviceItemLeftBudTrueWirelessBatteryPercentage',
            batteryPercentage),
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#leftBudBatteryPercentage')
            .innerText.trim());
    assertEquals(
        isLowBattery,
        bluetoothDeviceBatteryInfo.getIsLeftBudLowBatteryForTest());
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#leftBudBatteryIcon'));
    assertEquals(
        'bluetooth:battery-' + batteryIconRange,
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#leftBudBatteryIcon')
            .icon);

    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#caseBatteryPercentage'));
    assertEquals(
        bluetoothDeviceBatteryInfo.i18n(
            'bluetoothPairedDeviceItemCaseTrueWirelessBatteryPercentage',
            batteryPercentage),
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#caseBatteryPercentage')
            .innerText.trim());
    assertEquals(
        isLowBattery, bluetoothDeviceBatteryInfo.getIsCaseLowBatteryForTest());
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#caseBatteryIcon'));
    assertEquals(
        'bluetooth:battery-' + batteryIconRange,
        bluetoothDeviceBatteryInfo.shadowRoot.querySelector('#caseBatteryIcon')
            .icon);

    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#rightBudBatteryPercentage'));
    assertEquals(
        bluetoothDeviceBatteryInfo.i18n(
            'bluetoothPairedDeviceItemRightBudTrueWirelessBatteryPercentage',
            batteryPercentage),
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#rightBudBatteryPercentage')
            .innerText.trim());
    assertEquals(
        isLowBattery,
        bluetoothDeviceBatteryInfo.getIsRightBudLowBatteryForTest());
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#rightBudBatteryIcon'));
    assertEquals(
        'bluetooth:battery-' + batteryIconRange,
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#rightBudBatteryIcon')
            .icon);
  }

  test('Battery text, icon and color', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    bluetoothDeviceBatteryInfo.device = device.deviceProperties;

    // Lower bound edge case.
    let batteryPercentage = 0;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState(
        batteryPercentage, /*isLowBattery=*/ true, /*batteryIconRange=*/ '0-7');

    batteryPercentage = 3;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '0-7');

    // Maximum 'low battery' percentage.
    batteryPercentage = 24;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '22-28');

    // Minimum non-'low battery' percentage.
    batteryPercentage = 25;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '22-28');

    batteryPercentage = 94;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Upper bound edge case.
    batteryPercentage = 100;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Broken cases where the battery doesn't show.
    batteryPercentage = 110;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertEquals(
        '',
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#defaultBatteryPercentage')
            .innerText.trim());
    assertEquals(
        '',
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#defaultBatteryIcon')
            .icon);
    batteryPercentage = -5;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertEquals(
        '',
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#defaultBatteryPercentage')
            .innerText.trim());
    assertEquals(
        '',
        bluetoothDeviceBatteryInfo.shadowRoot
            .querySelector('#defaultBatteryIcon')
            .icon);
  });

  test('Battery text, icon and color multiple batteries', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    bluetoothDeviceBatteryInfo.device = device.deviceProperties;

    // Lower bound edge case.
    let batteryPercentage = 0;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState(
        batteryPercentage, /*isLowBattery=*/ true, /*batteryIconRange=*/ '0-7');

    batteryPercentage = 3;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '0-7');

    // Maximum 'low battery' percentage.
    batteryPercentage = 24;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '22-28');

    // Minimum non-'low battery' percentage.
    batteryPercentage = 25;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '22-28');

    batteryPercentage = 94;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Upper bound edge case.
    batteryPercentage = 100;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Broken cases where the battery doesn't show.
    batteryPercentage = 110;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#leftBudBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#caseBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#rightBudBatteryPercentage'));

    batteryPercentage = -5;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#leftBudBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#caseBatteryPercentage'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot.querySelector(
        '#rightBudBatteryPercentage'));
  });
});
