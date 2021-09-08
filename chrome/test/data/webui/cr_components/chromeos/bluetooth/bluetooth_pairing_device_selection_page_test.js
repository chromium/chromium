// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingDeviceSelectionPageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_device_selection_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import { assertEquals, assertFalse,assertTrue} from '../../../chai_assert.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingDeviceSelectionPageTest', function() {
  /** @type {?SettingsBluetoothPairingDeviceSelectionPageElement} */
  let deviceSelectionPage;

  setup(function() {
    deviceSelectionPage =
        /** @type {?SettingsBluetoothPairingDeviceSelectionPageElement} */ (
            document.createElement('bluetooth-pairing-device-selection-page'));
    document.body.appendChild(deviceSelectionPage);
    flush();
  });

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Device lists states', async function() {
    const getDeviceList = () =>
        deviceSelectionPage.shadowRoot.querySelector('iron-list');
    const getDeviceListTitle = () =>
        deviceSelectionPage.shadowRoot.querySelector('#deviceListTitle');

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
        /*nickname=*/ 'device1',
        /*audioCapability=*/ mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*deviceType=*/ mojom.DeviceType.kMouse);

    deviceSelectionPage.devices = [device.deviceProperties];

    await flushAsync();

    assertTrue(!!getDeviceList());
    assertEquals(getDeviceList().items.length, 1);
    assertEquals(
        deviceSelectionPage.i18n('bluetoothAvailableDevices'),
        getDeviceListTitle().textContent.trim());
  });
});
