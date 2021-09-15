// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// clang-format on

suite('OsBluetoothChangeDeviceNameDialogTest', function() {
  /** @type {!SettingsBluetoothChangeDeviceNameDialogElement|undefined} */
  let bluetoothDeviceChangeNameDialog;

  setup(function() {
    bluetoothDeviceChangeNameDialog = document.createElement(
        'os-settings-bluetooth-change-device-name-dialog');
    document.body.appendChild(bluetoothDeviceChangeNameDialog);
    Polymer.dom.flush();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise((resolve) => setTimeout(resolve));
  }

  /**
   * @param {string} value The value of the input
   * @param {boolean} invalid If the input is invalid or not
   * @param {string} inputCount The length of value in string
   *     format, with 2 digits
   */
  function assertInput(value, invalid, valueLength) {
    const input = bluetoothDeviceChangeNameDialog.$$('#changeNameInput');
    const inputCount = bluetoothDeviceChangeNameDialog.$$('#inputCount');
    assertTrue(!!input);
    assertTrue(!!inputCount);

    assertEquals(input.value, value);
    assertEquals(input.invalid, invalid);
    const characterCountText = bluetoothDeviceChangeNameDialog.i18n(
        'bluetoothChangeNameDialogInputCharCount', valueLength, 20);
    assertEquals(inputCount.textContent.trim(), characterCountText);
    assertEquals(
        input.ariaDescription,
        bluetoothDeviceChangeNameDialog.i18n(
            'bluetoothChangeNameDialogInputA11yLabel', 20));
  }

  test('Base Test', async function() {
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.device = {...device1};
    await flushAsync();

    const input = bluetoothDeviceChangeNameDialog.$$('#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);
  });

  test('Input is sanitized', async function() {
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.device = {...device1};
    await flushAsync();

    await flushAsync();
    const input = bluetoothDeviceChangeNameDialog.$$('#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);

    // Test empty name.
    input.value = '';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // // Test name with no emojis, under character limit.
    input.value = '1234567890123456789';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');

    // Test name with emojis, under character limit.
    input.value = '1234😀5678901234🧟';
    assertInput(
        /*value=*/ '12345678901234', /*invalid=*/ false,
        /*valueLength=*/ '14');

    // // Test name with only emojis, under character limit.
    input.value = '😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name with no emojis, at character limit.
    input.value = '12345678901234567890';
    assertInput(
        /*value=*/ '12345678901234567890', /*invalid=*/ false,
        /*valueLength=*/ '20');

    // Test name with emojis, at character limit.
    input.value = '1234567890123456789🧟';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');

    // Test name with only emojis, at character limit.
    input.value = '😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name with no emojis, above character limit.
    input.value = '123456789012345678901';
    assertInput(
        /*value=*/ '12345678901234567890', /*invalid=*/ true,
        /*valueLength=*/ '20');

    // Make sure input is not invalid once its value changes to a string below
    // the character limit. (Simulates the user pressing backspace once they've
    // reached the limit).
    input.value = '1234567890123456789';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');

    // Test name with emojis, above character limit.
    input.value = '12345678901234567890🧟';
    assertInput(
        /*value=*/ '12345678901234567890', /*invalid=*/ false,
        /*valueLength=*/ '20');

    // Test name with only emojis, above character limit.
    input.value = '😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');
  });
});
