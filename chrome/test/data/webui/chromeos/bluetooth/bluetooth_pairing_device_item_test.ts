// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_device_item.js';

import type {SettingsBluetoothPairingDeviceItemElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_device_item.js';
import {DeviceItemState} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

suite('CrComponentsBluetoothPairingDeviceItemTest', function() {
  let bluetoothPairingDeviceItem: SettingsBluetoothPairingDeviceItemElement;

  async function flushAsync(): Promise<void> {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothPairingDeviceItem =
        document.createElement('bluetooth-pairing-device-item');
    document.body.appendChild(bluetoothPairingDeviceItem);
    assertTrue(!!bluetoothPairingDeviceItem);
    flush();
  });

  test('Correct device name is shown', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothPairingDeviceItem.device = device.deviceProperties;
    await flushAsync();

    const deviceName =
        bluetoothPairingDeviceItem.shadowRoot!.querySelector('#deviceName');
    assertTrue(!!deviceName);
    // deviceName uses ! flag because the compilar currently fails when
    // running test locally.
    assertEquals('BeatsX', deviceName!.textContent!.trim());
  });

  test('pair-device is fired on click or enter', async function() {
    let pairToDevicePromise =
        eventToPromise('pair-device', bluetoothPairingDeviceItem);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*nickname=*/ 'device1',
        /*audioCapability=*/ AudioOutputCapability.kCapableOfAudioOutput,
        /*deviceType=*/ DeviceType.kMouse);

    bluetoothPairingDeviceItem.device = device.deviceProperties;
    await flushAsync();

    const container =
        bluetoothPairingDeviceItem.shadowRoot!.querySelector<HTMLElement>(
            '#container');

    assertTrue(!!container);
    // container uses ! flag because the compilar currently fails when
    // running test locally.
    container!.click();
    await pairToDevicePromise;

    // Simulate pressing enter on the item.
    pairToDevicePromise =
        eventToPromise('pair-device', bluetoothPairingDeviceItem);
    container!.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Enter'}));
    await pairToDevicePromise;
  });

  test('Pairing message is shown', async function() {
    const deviceName = 'BeatsX';
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789', deviceName,
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*nickname=*/ 'device1',
        /*audioCapability=*/ AudioOutputCapability.kCapableOfAudioOutput,
        /*deviceType=*/ DeviceType.kMouse);

    bluetoothPairingDeviceItem.device = device.deviceProperties;
    await flushAsync();

    const itemIndex = 1;
    const listSize = 10;
    bluetoothPairingDeviceItem.itemIndex = itemIndex;
    bluetoothPairingDeviceItem.listSize = listSize;

    const getSecondaryLabel = () => bluetoothPairingDeviceItem.$.secondaryLabel;
    const getItemSecondaryA11yLabel = () =>
        bluetoothPairingDeviceItem.$.textRow.ariaLabel;
    const getItemA11yLabel = () =>
        bluetoothPairingDeviceItem.$.container.ariaLabel;

    assertTrue(!!getSecondaryLabel());
    assertEquals('', getSecondaryLabel().textContent!.trim());

    const expectedA11yLabel =
        bluetoothPairingDeviceItem.i18n(
            'bluetoothA11yDeviceName', itemIndex + 1, listSize, deviceName) +
        ' ' + bluetoothPairingDeviceItem.i18n('bluetoothA11yDeviceTypeMouse');
    assertEquals(getItemA11yLabel(), expectedA11yLabel);
    assertEquals(getItemSecondaryA11yLabel(), '');

    bluetoothPairingDeviceItem.deviceItemState = DeviceItemState.PAIRING;
    await flushAsync();

    assertEquals(
        bluetoothPairingDeviceItem.i18n('bluetoothPairing'),
        getSecondaryLabel().textContent!.trim());
    assertEquals(
        getItemSecondaryA11yLabel(),
        bluetoothPairingDeviceItem.i18n(
            'bluetoothPairingDeviceItemSecondaryPairingA11YLabel', deviceName));

    bluetoothPairingDeviceItem.deviceItemState = DeviceItemState.FAILED;
    await flushAsync();

    assertEquals(
        bluetoothPairingDeviceItem.i18n('bluetoothPairingFailed'),
        getSecondaryLabel().textContent!.trim());
    assertEquals(
        getItemSecondaryA11yLabel(),
        bluetoothPairingDeviceItem.i18n(
            'bluetoothPairingDeviceItemSecondaryErrorA11YLabel', deviceName));

    bluetoothPairingDeviceItem.deviceItemState = DeviceItemState.DEFAULT;
    await flushAsync();
    assertEquals('', getSecondaryLabel().textContent!.trim());
  });
});
