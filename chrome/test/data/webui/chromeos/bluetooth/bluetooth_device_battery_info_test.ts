// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_device_battery_info.js';

import type {BluetoothDeviceBatteryInfoElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_device_battery_info.js';
import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from '../chai_assert.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

suite('CrComponentsBluetoothDeviceBatteryInfoTest', function() {
  let bluetoothDeviceBatteryInfo: BluetoothDeviceBatteryInfoElement;

  setup(function() {
    bluetoothDeviceBatteryInfo =
        document.createElement('bluetooth-device-battery-info');
    document.body.appendChild(bluetoothDeviceBatteryInfo);
    flush();
  });

  function flushAsync(): Promise<null> {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  async function setDefaultBatteryPercentage(batteryPercentage: number):
      Promise<null> {
    bluetoothDeviceBatteryInfo.device.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage},
      leftBudInfo: undefined,
      rightBudInfo: undefined,
      caseInfo: undefined,
    };
    bluetoothDeviceBatteryInfo.device =
        Object.assign({}, bluetoothDeviceBatteryInfo.device);
    return flushAsync();
  }

  async function setMultipleBatteryPercentage(batteryPercentage: number):
      Promise<null> {
    bluetoothDeviceBatteryInfo.device.batteryInfo = {
      defaultProperties: undefined,
      leftBudInfo: {batteryPercentage: batteryPercentage},
      caseInfo: {batteryPercentage: batteryPercentage},
      rightBudInfo: {batteryPercentage: batteryPercentage},
    };
    bluetoothDeviceBatteryInfo.device =
        Object.assign({}, bluetoothDeviceBatteryInfo.device);
    return flushAsync();
  }

  function assertDefaultBatteryUIState(): void {
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector(
        '#defaultBattery'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector(
        '#leftBudBattery'));
    assertFalse(
        !!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector('#caseBattery'));
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector(
        '#rightBudBattery'));
  }

  function assertMultipleBatteryUIState(): void {
    assertFalse(!!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector(
        '#defaultBattery'));
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector(
        '#leftBudBattery'));
    assertTrue(
        !!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector('#caseBattery'));
    assertTrue(!!bluetoothDeviceBatteryInfo.shadowRoot!.querySelector(
        '#rightBudBattery'));
  }

  test('Default battery state', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    bluetoothDeviceBatteryInfo.device = device.deviceProperties;

    let batteryPercentage = 0;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState();

    batteryPercentage = 100;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState();

    // We don't handle illegal battery percentages in this component.
    batteryPercentage = 105;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState();

    batteryPercentage = -1;
    await setDefaultBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState();
  });

  test('Multiple battery state', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    bluetoothDeviceBatteryInfo.device = device.deviceProperties;

    let batteryPercentage = 0;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState();

    batteryPercentage = 100;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertMultipleBatteryUIState();

    // Revert to default UI in case of illegal battery percentage.
    batteryPercentage = 105;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState();

    batteryPercentage = -1;
    await setMultipleBatteryPercentage(batteryPercentage);
    assertDefaultBatteryUIState();
  });
});
