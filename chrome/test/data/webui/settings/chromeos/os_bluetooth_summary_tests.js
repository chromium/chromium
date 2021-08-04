// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeBluetoothConfig} from './fake_bluetooth_config.m.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// clang-format on

suite('OsBluetoothSummaryTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothSummaryElement|undefined} */
  let bluetoothSummary;

  /**
   * @type {!chromeos.bluetoothConfig.mojom.SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothSummary = document.createElement('os-settings-bluetooth-summary');
    document.body.appendChild(bluetoothSummary);
    Polymer.dom.flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothSummary.systemProperties = properties;
      }
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
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
    const enableBluetoothToggle = bluetoothSummary.$$('#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);
    assertFalse(enableBluetoothToggle.checked);

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushAsync();

    // Toggle should be off again.
    assertFalse(enableBluetoothToggle.checked);

    // Click again.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushAsync();

    // Toggle should still be on.
    assertTrue(enableBluetoothToggle.checked);

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertTrue(enableBluetoothToggle.disabled);
  });
});