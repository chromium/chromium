// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeBluetoothConfig} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

suite('OsBluetoothPageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothPageElement|undefined} */
  let bluetoothPage;

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothPage = document.createElement('os-settings-bluetooth-page');
    document.body.appendChild(bluetoothPage);
    Polymer.dom.flush();
  });

  test('Show bluetooth pairing UI', async function() {
    const getBluetoothPairingUi = () =>
        bluetoothPage.$$('os-settings-bluetooth-pairing-dialog');
    const bluetoothSummary = bluetoothPage.$$('os-settings-bluetooth-summary');

    const getPairNewDevice = () => bluetoothPage.$$('#pairNewDevice');

    assertTrue(!!bluetoothSummary);
    assertFalse(!!getBluetoothPairingUi());

    bluetoothSummary.dispatchEvent(new CustomEvent('start-pairing'));

    await flushAsync();
    assertTrue(!!getBluetoothPairingUi());

    getBluetoothPairingUi().dispatchEvent(new CustomEvent('close'));

    await flushAsync();
    assertFalse(!!getBluetoothPairingUi());

    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICES, null);

    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushAsync();
    assertTrue(!!getPairNewDevice());

    // Simulate Bluetooth disabled
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertFalse(!!getPairNewDevice());

    // Simulate Bluetooth enabled
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushAsync();
    assertTrue(!!getPairNewDevice());
    getPairNewDevice().click();

    await flushAsync();
    assertTrue(!!getBluetoothPairingUi());

  });
});