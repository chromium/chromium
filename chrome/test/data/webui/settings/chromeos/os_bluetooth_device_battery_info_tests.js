// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// clang-format on

suite('OsBluetoothDeviceBatteryInfoTest', function() {
  /** @type {!SettingsOsBluetoothDeviceBatteryInfoElement|undefined} */
  let bluetoothDeviceBatteryInfo;

  setup(function() {
    bluetoothDeviceBatteryInfo =
        document.createElement('os-settings-bluetooth-device-battery-info');
    document.body.appendChild(bluetoothDeviceBatteryInfo);
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
    bluetoothDeviceBatteryInfo.device.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage}
    };
    bluetoothDeviceBatteryInfo.device = {...bluetoothDeviceBatteryInfo.device};
    return flushAsync();
  }

  /**
   * @param {number} batteryPercentage
   * @param {boolean} isLowBattery
   * @param {boolean} batteryIconRange
   */
  function assertBatteryUIState(
      batteryPercentage, isLowBattery, batteryIconRange) {
    assertEquals(
        bluetoothDeviceBatteryInfo.$.batteryPercentage.innerText.trim(),
        bluetoothDeviceBatteryInfo.i18n(
            'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage));
    assertEquals(bluetoothDeviceBatteryInfo.isLowBattery_, isLowBattery);
    assertEquals(
        bluetoothDeviceBatteryInfo.shadowRoot.querySelector('#batteryIcon')
            .icon,
        'os-settings:battery-' + batteryIconRange);
  }

  test('Battery text, icon and color', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX', /*connected=*/ true);
    bluetoothDeviceBatteryInfo.device = device;

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