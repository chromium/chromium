// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {BluetoothDeviceBatteryInfoElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_device_battery_info.js';
import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from '../../../chai_assert.js';
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
  async function setBatteryPercentage(batteryPercentage) {
    bluetoothDeviceBatteryInfo.device.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage}
    };
    bluetoothDeviceBatteryInfo.device =
        /**
          @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
        */
        (Object.assign({}, bluetoothDeviceBatteryInfo.device));
    return flushAsync();
  }

  /**
   * @param {number} batteryPercentage
   * @param {boolean} isLowBattery
   * @param {string} batteryIconRange
   */
  function assertBatteryUIState(
      batteryPercentage, isLowBattery, batteryIconRange) {
    assertEquals(
        bluetoothDeviceBatteryInfo.$.batteryPercentage.innerText.trim(),
        bluetoothDeviceBatteryInfo.i18n(
            'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage));
    assertEquals(
        bluetoothDeviceBatteryInfo.getIsLowBatteryForTest(), isLowBattery);
    assertEquals(
        bluetoothDeviceBatteryInfo.shadowRoot.querySelector('#batteryIcon')
            .icon,
        'bluetooth:battery-' + batteryIconRange);
  }

  test('Battery text, icon and color', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX', /*connected=*/ true);
    bluetoothDeviceBatteryInfo.device = device.deviceProperties;

    // Lower bound edge case.
    let batteryPercentage = 0;
    await setBatteryPercentage(batteryPercentage);
    assertBatteryUIState(
        batteryPercentage, /*isLowBattery=*/ true, /*batteryIconRange=*/ '0-7');

    batteryPercentage = 3;
    await setBatteryPercentage(batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '0-7');

    // Maximum 'low battery' percentage.
    batteryPercentage = 24;
    await setBatteryPercentage(batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ true,
        /*batteryIconRange=*/ '22-28');

    // Minimum non-'low battery' percentage.
    batteryPercentage = 25;
    await setBatteryPercentage(batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '22-28');

    batteryPercentage = 94;
    await setBatteryPercentage(batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');

    // Upper bound edge case.
    batteryPercentage = 100;
    await setBatteryPercentage(batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /*isLowBattery=*/ false,
        /*batteryIconRange=*/ '93-100');
  });
});