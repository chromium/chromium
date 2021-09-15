// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingDeviceSelectionPageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_device_selection_page.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from './fake_bluetooth_config.js';
import {FakeBluetoothDiscoveryDelegate} from './fake_bluetooth_discovery_delegate.js';

// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingDeviceSelectionPageTest', function() {
  /** @type {?SettingsBluetoothPairingDeviceSelectionPageElement} */
  let deviceSelectionPage;

  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /**
   * @type {!FakeBluetoothDiscoveryDelegate}
   */
  let discoveryDelegate;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);

    deviceSelectionPage =
        /** @type {?SettingsBluetoothPairingDeviceSelectionPageElement} */ (
            document.createElement('bluetooth-pairing-device-selection-page'));
    document.body.appendChild(deviceSelectionPage);
    flush();

    discoveryDelegate = new FakeBluetoothDiscoveryDelegate();
    discoveryDelegate.addDeviceListChangedCallback(onDeviceListChanged);
    bluetoothConfig.startDiscovery(discoveryDelegate);
  });

  /**
   * @param {!Array<!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties>}
   *     discoveredDevices
   */
  function onDeviceListChanged(discoveredDevices) {
    deviceSelectionPage.devices = discoveredDevices;
  }

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Device lists states', async function() {
    const getDeviceList = () =>
        deviceSelectionPage.shadowRoot.querySelector('iron-list');
    const getDeviceListTitle = () =>
        deviceSelectionPage.shadowRoot.querySelector('#deviceListTitle');

    const learnMoreLink =
        deviceSelectionPage.shadowRoot.querySelector('localized-link');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.localizedString,
        deviceSelectionPage.i18nAdvanced('bluetoothPairingLearnMoreLabel'));
    // No lists should be showing at first.
    assertFalse(!!getDeviceList());
    assertTrue(!!getDeviceListTitle());

    assertEquals(
        deviceSelectionPage.i18n('bluetoothNoAvailableDevices'),
        getDeviceListTitle().textContent.trim());

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushAsync();

    assertTrue(!!getDeviceList());
    assertEquals(getDeviceList().items.length, 1);
    assertEquals(
        deviceSelectionPage.i18n('bluetoothAvailableDevices'),
        getDeviceListTitle().textContent.trim());
  });
});
