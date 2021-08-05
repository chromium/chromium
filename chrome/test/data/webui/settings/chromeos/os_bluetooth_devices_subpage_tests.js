// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeBluetoothConfig} from './fake_bluetooth_config.m.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// clang-format on

suite('OsBluetoothDevicesSubpageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothDevicesSubpageElement|undefined} */
  let bluetoothDevicesSubpage;

  /**
   * @type {!chromeos.bluetoothConfig.mojom.SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    document.body.appendChild(bluetoothDevicesSubpage);
    Polymer.dom.flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothDevicesSubpage.systemProperties = properties;
      }
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', function() {
    assertTrue(!!bluetoothDevicesSubpage);
  });

  test('Toggle button states', async function() {
    const enableBluetoothToggle =
        bluetoothDevicesSubpage.$.enableBluetoothToggle;
    assertTrue(!!enableBluetoothToggle);

    const assertToggleEnabledState = (enabled) => {
      assertEquals(enableBluetoothToggle.checked, enabled);
      assertEquals(
          bluetoothDevicesSubpage.$.onOff.innerText,
          bluetoothDevicesSubpage.i18n(enabled ? 'deviceOn' : 'deviceOff'));
    };
    assertToggleEnabledState(/*enabled=*/ false);

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushAsync();

    // Toggle should be off again.
    assertToggleEnabledState(/*enabled=*/ false);

    // Click again.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushAsync();

    // Toggle should still be on.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertTrue(enableBluetoothToggle.disabled);
  });
});