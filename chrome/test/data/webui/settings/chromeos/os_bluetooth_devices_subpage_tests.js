// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
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

  /** @type {!chromeos.bluetoothConfig.mojom} */
  let mojom;

  setup(function() {
    mojom = chromeos.bluetoothConfig.mojom;

    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  function init() {
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
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', function() {
    init();
    assertTrue(!!bluetoothDevicesSubpage);
  });

  test('Toggle button creation', async function() {
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushAsync();
    init();
    assertTrue(bluetoothDevicesSubpage.$.enableBluetoothToggle.checked);
  });

  test('Toggle button states', async function() {
    init();

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
    bluetoothConfig.setSystemState(mojom.BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertTrue(enableBluetoothToggle.disabled);
  });

  test('Device lists states', async function() {
    init();

    const getNoDeviceText = () =>
        bluetoothDevicesSubpage.shadowRoot.querySelector('#noDevices');

    const getDeviceList = (connected) => {
      return bluetoothDevicesSubpage.shadowRoot.querySelector(
          connected ? '#connectedDeviceList' : '#unconnectedDeviceList');
    };
    // No lists should be showing at first.
    assertFalse(!!getDeviceList(/*connected=*/ true));
    assertFalse(!!getDeviceList(/*connected=*/ false));
    assertTrue(!!getNoDeviceText());
    assertEquals(
        getNoDeviceText().textContent.trim(),
        bluetoothDevicesSubpage.i18n('bluetoothDeviceListNoConnectedDevices'));

    const connectedDevice = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    const unconnectedDevice = createDefaultBluetoothDevice(
        /*id=*/ '987654321', /*publicName=*/ 'MX 3',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kNotConnected);

    // Pair connected device.
    bluetoothConfig.appendToPairedDeviceList([connectedDevice]);
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertFalse(!!getDeviceList(/*connected=*/ false));
    assertFalse(!!getNoDeviceText());

    // Pair unconnected device
    bluetoothConfig.appendToPairedDeviceList([unconnectedDevice]);
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(getDeviceList(/*connected=*/ false).devices.length, 1);
    assertFalse(!!getNoDeviceText());
  });
});
