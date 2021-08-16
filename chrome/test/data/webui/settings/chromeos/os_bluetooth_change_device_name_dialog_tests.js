// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice} from './fake_bluetooth_config.m.js';
// clang-format on

suite('OsBluetoothDeviceDetailPageTest', function() {
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

  test('Base Test', async function() {
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.device = {...device1};
    await flushAsync();

    const input = bluetoothDeviceChangeNameDialog.$$('#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);
  });
});
