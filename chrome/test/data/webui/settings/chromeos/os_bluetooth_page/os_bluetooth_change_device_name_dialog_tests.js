// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/strings.m.js';

import {getDeviceName} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';

suite('OsBluetoothChangeDeviceNameDialogTest', function() {
  /** @type {!SettingsBluetoothChangeDeviceNameDialogElement|undefined} */
  let bluetoothDeviceChangeNameDialog;

  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothDeviceChangeNameDialog = document.createElement(
        'os-settings-bluetooth-change-device-name-dialog');
    document.body.appendChild(bluetoothDeviceChangeNameDialog);
    flush();
  });

  function flushAsync() {
    flush();
    return new Promise((resolve) => setTimeout(resolve));
  }

  /**
   * @param {string} value The value of the input
   * @param {boolean} invalid If the input is invalid or not
   * @param {string} valueLength The length of value in string
   *     format, with 2 digits
   */
  function assertInput(value, invalid, valueLength) {
    const input = bluetoothDeviceChangeNameDialog.shadowRoot.querySelector(
        '#changeNameInput');
    const inputCount =
        bluetoothDeviceChangeNameDialog.shadowRoot.querySelector('#inputCount');
    assertTrue(!!input);
    assertTrue(!!inputCount);

    assertEquals(input.value, value);
    assertEquals(input.invalid, invalid);
    const characterCountText = bluetoothDeviceChangeNameDialog.i18n(
        'bluetoothChangeNameDialogInputCharCount', valueLength, 32);
    assertEquals(inputCount.textContent.trim(), characterCountText);
    assertEquals(
        input.ariaDescription,
        bluetoothDeviceChangeNameDialog.i18n(
            'bluetoothChangeNameDialogInputA11yLabel', 32));
  }

  test('Input is sanitized', async function() {
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.device = {...device1};
    await flushAsync();

    await flushAsync();
    const input = bluetoothDeviceChangeNameDialog.shadowRoot.querySelector(
        '#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);

    // Test empty name.
    input.value = '';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name, under character limit.
    input.value = '1234567890123456789';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');


    // Test name, at character limit.
    input.value = '12345678901234567890123456789012';
    assertInput(
        /*value=*/ '12345678901234567890123456789012', /*invalid=*/ false,
        /*valueLength=*/ '32');

    // Test name, above character limit.
    input.value = '123456789012345678901234567890123';
    assertInput(
        /*value=*/ '12345678901234567890123456789012', /*invalid=*/ true,
        /*valueLength=*/ '32');

    // Make sure input is not invalid once its value changes to a string below
    // the character limit. (Simulates the user pressing backspace once they've
    // reached the limit).
    input.value = '1234567890123456789012345678901';
    assertInput(
        /*value=*/ '1234567890123456789012345678901', /*invalid=*/ false,
        /*valueLength=*/ '31');
  });

  test('Device name is changed', async function() {
    const id = '12//345&6789';
    const nickname = 'Nickname';
    const getDoneBtn = () =>
        bluetoothDeviceChangeNameDialog.shadowRoot.querySelector('#done');
    const device = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.device = {...device};
    bluetoothConfig.appendToPairedDeviceList([device]);
    await flushAsync();

    const input = bluetoothDeviceChangeNameDialog.shadowRoot.querySelector(
        '#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);
    assertTrue(getDoneBtn().disabled);

    input.value = nickname;
    await flushAsync();
    assertFalse(getDoneBtn().disabled);

    getDoneBtn().click();
    await flushAsync();

    const newName = getDeviceName(bluetoothConfig.getPairedDeviceById(id));

    assertEquals(newName, nickname);
  });
});
