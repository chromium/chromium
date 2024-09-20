// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/ash/common/bluetooth/bluetooth_icon.js';

import type {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import type {SettingsBluetoothIconElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_icon.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

suite('CrComponentsBluetoothIconTest', function() {
  let bluetoothIcon: SettingsBluetoothIconElement;

  async function flushAsync(): Promise<null> {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothIcon = document.createElement('bluetooth-icon');
    document.body.appendChild(bluetoothIcon);
    assertTrue(!!bluetoothIcon);
    flush();
  });

  test('Correct icon is shown', async function() {
    const getDefaultImage = () =>
        bluetoothIcon.shadowRoot!.querySelector('#image');
    const getDeviceIcon = () =>
        bluetoothIcon.shadowRoot!.querySelector<IronIconElement>(
            '#deviceTypeIcon');
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothIcon.device = device.deviceProperties;
    await flushAsync();

    let deviceIcon = getDeviceIcon();
    assertTrue(!!deviceIcon);
    assertFalse(!!getDefaultImage());
    // deviceIcon uses ! flag because the compilar currently fails when
    // running test locally.
    assertEquals(deviceIcon!.icon, 'bluetooth:mouse');

    device.deviceProperties.deviceType = DeviceType.kUnknown;

    bluetoothIcon.device = Object.assign({}, device.deviceProperties);
    await flushAsync();

    deviceIcon = getDeviceIcon();
    assertTrue(!!deviceIcon);
    // deviceIcon uses ! flag because the compilar currently fails when
    // running test locally.
    assertEquals(deviceIcon!.icon, 'bluetooth:default');
  });

  test('Displays default image when available', async function() {
    const getDefaultImage = () =>
        bluetoothIcon.shadowRoot!.querySelector('#image');
    const getDeviceIcon = () =>
        bluetoothIcon.shadowRoot!.querySelector('#deviceTypeIcon');
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/ DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/ AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    const fakeUrl = 'fake_image';
    device.deviceProperties.imageInfo = {
      defaultImageUrl: {
        url: fakeUrl,
      },
      trueWirelessImages: undefined,
    };

    bluetoothIcon.device = device.deviceProperties;
    await flushAsync();

    assertTrue(!!getDefaultImage());
    assertFalse(!!getDeviceIcon());
  });
});
