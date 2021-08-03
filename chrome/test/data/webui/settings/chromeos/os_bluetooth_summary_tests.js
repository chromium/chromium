// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// clang-format on

suite('OsBluetoothSummaryTest', function() {
  /** @type {!SettingsBluetoothSummaryElement|undefined} */
  let bluetoothSummary;

  setup(function() {
    // TODO(crbug.com/1010321): Replace this with fake_cros_bluetooth_config
    // when it is created.
    setBluetoothConfigForTesting({setBluetoothEnabledState: (enabled) => {}});
    bluetoothSummary = document.createElement('os-settings-bluetooth-summary');
    document.body.appendChild(bluetoothSummary);
    Polymer.dom.flush();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Route to Bluetooth devices subpage', async function() {
    const iconButton = bluetoothSummary.$$('#iconButton');
    assertTrue(!!iconButton);
    iconButton.click();

    await flushAsync();
    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.BLUETOOTH_DEVICES);
  });

  test('Toggle button states', async function() {
    // TODO(crbug.com/1010321): Remove |mockSystemProperties| once
    // fake_cros_bluetooth_config has been added.
    const mockSystemProperties = {
      systemState: chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled,
    };
    bluetoothSummary.systemProperties = {...mockSystemProperties};

    const enableBluettonToggle = bluetoothSummary.$$('#enableBluetoothToggle');
    assertTrue(!!enableBluettonToggle);
    assertTrue(enableBluettonToggle.checked);

    // Simulate disabled state.
    mockSystemProperties.systemState =
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kDisabled;
    bluetoothSummary.systemProperties = {...mockSystemProperties};
    await flushAsync();

    assertFalse(enableBluettonToggle.checked);
  });

});