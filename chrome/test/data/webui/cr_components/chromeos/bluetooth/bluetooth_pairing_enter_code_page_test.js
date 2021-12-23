// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingEnterCodeElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_enter_code_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../../chai_assert.js';
import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingEnterCodePageTest', function() {
  /** @type {?SettingsBluetoothPairingEnterCodeElement} */
  let bluetoothPairingEnterCodePage;

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothPairingEnterCodePage =
        /** @type {?SettingsBluetoothPairingEnterCodeElement} */ (
            document.createElement('bluetooth-pairing-enter-code-page'));
    document.body.appendChild(bluetoothPairingEnterCodePage);
    assertTrue(!!bluetoothPairingEnterCodePage);
    flush();
  });

  test('UI states', async function() {
    const getKeys = () =>
        bluetoothPairingEnterCodePage.shadowRoot.querySelectorAll('.key');
    const getEnter = () =>
        bluetoothPairingEnterCodePage.shadowRoot.querySelector('#enter');

    const deviceName = 'BeatsX';
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456', deviceName,
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothPairingEnterCodePage.device = device.deviceProperties;
    await flushAsync();

    const message =
        bluetoothPairingEnterCodePage.shadowRoot.querySelector('#message');

    assertEquals(
        bluetoothPairingEnterCodePage.i18n(
            'bluetoothPairingEnterKeys', deviceName),
        message.textContent.trim());

    const defaultKeyClass = 'center key ';
    const nextKeyClass = defaultKeyClass + 'next';
    const typedKeyClass = defaultKeyClass + 'typed';
    const defaultEnterClass = 'center enter ';
    const nextEnterClass = defaultEnterClass + 'next';

    bluetoothPairingEnterCodePage.code = '123456';
    bluetoothPairingEnterCodePage.numKeysEntered = 0;
    await flushAsync();

    let keys = getKeys();
    assertEquals(keys.length, 6);
    assertEquals(keys[0].className, defaultKeyClass);
    assertEquals(keys[1].className, defaultKeyClass);
    assertEquals(keys[5].className, defaultKeyClass);
    assertEquals(getEnter().className, defaultEnterClass);

    bluetoothPairingEnterCodePage.numKeysEntered = 2;
    await flushAsync();

    keys = getKeys();
    assertEquals(keys[2].className, nextKeyClass);
    assertEquals(keys[1].className, typedKeyClass);
    assertEquals(keys[5].className, defaultKeyClass);
    assertEquals(getEnter().className, defaultEnterClass);

    bluetoothPairingEnterCodePage.numKeysEntered = 6;
    await flushAsync();

    keys = getKeys();
    assertEquals(keys[1].className, typedKeyClass);
    assertEquals(keys[5].className, typedKeyClass);
    assertEquals(getEnter().className, nextEnterClass);
  });
});