// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingDeviceItemElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_device_item.js';
import {DeviceItemState} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../../chai_assert.js';
import {eventToPromise} from '../../../test_util.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingDeviceItemTest', function() {
  /** @type {?SettingsBluetoothPairingDeviceItemElement} */
  let bluetoothPairingDeviceItem;

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothPairingDeviceItem =
        /** @type {?SettingsBluetoothPairingDeviceItemElement} */ (
            document.createElement('bluetooth-pairing-device-item'));
    document.body.appendChild(bluetoothPairingDeviceItem);
    assertTrue(!!bluetoothPairingDeviceItem);
    flush();
  });

  test('Correct device name is shown', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothPairingDeviceItem.device = device.deviceProperties;
    await flushAsync();

    const deviceName =
        bluetoothPairingDeviceItem.shadowRoot.querySelector('#deviceName');
    assertTrue(!!deviceName);
    assertEquals('BeatsX', deviceName.textContent.trim());
  });

  test('pair-device is fired on click or enter', async function() {
    let pairToDevicePromise =
        eventToPromise('pair-device', bluetoothPairingDeviceItem);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*nickname=*/ 'device1',
        /*audioCapability=*/ mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothPairingDeviceItem.device = device.deviceProperties;
    await flushAsync();

    const container =
        bluetoothPairingDeviceItem.shadowRoot.querySelector('#container');

    assertTrue(!!container);
    container.click();
    await pairToDevicePromise;

    // Simulate pressing enter on the item.
    pairToDevicePromise =
        eventToPromise('pair-device', bluetoothPairingDeviceItem);
    container.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Enter'}));
    await pairToDevicePromise;
  });

  test('Pairing message is shown', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*nickname=*/ 'device1',
        /*audioCapability=*/ mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothPairingDeviceItem.device = device.deviceProperties;
    await flushAsync();

    const getSecondaryLabel = () =>
        bluetoothPairingDeviceItem.shadowRoot.querySelector('#secondaryLabel');

    assertTrue(!!getSecondaryLabel());
    assertEquals('', getSecondaryLabel().textContent.trim());

    bluetoothPairingDeviceItem.deviceItemState = DeviceItemState.PAIRING;
    await flushAsync();

    assertEquals(
        bluetoothPairingDeviceItem.i18n('bluetoothPairing'),
        getSecondaryLabel().textContent.trim());

    bluetoothPairingDeviceItem.deviceItemState = DeviceItemState.FAILED;
    await flushAsync();

    assertEquals(
        bluetoothPairingDeviceItem.i18n('bluetoothPairingFailed'),
        getSecondaryLabel().textContent.trim());

    bluetoothPairingDeviceItem.deviceItemState = DeviceItemState.DEFAULT;
    await flushAsync();
    assertEquals('', getSecondaryLabel().textContent.trim());
  });
});