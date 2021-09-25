// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothRequestCodePageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_request_code_page.js';
import {ButtonState} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from '../../../chai_assert.js';
import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';
// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingRequestCodePageTest', function() {
  /** @type {?SettingsBluetoothRequestCodePageElement} */
  let bluetoothPairingRequestCodePage;

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothPairingRequestCodePage =
        /** @type {?SettingsBluetoothRequestCodePageElement} */ (
            document.createElement('bluetooth-pairing-request-code-page'));
    document.body.appendChild(bluetoothPairingRequestCodePage);
    assertTrue(!!bluetoothPairingRequestCodePage);
    flush();
  });

  test('Message and button states test', async function() {
    const getInput = () =>
        bluetoothPairingRequestCodePage.shadowRoot.querySelector('#pin');
    const getPairButtonState = () => {
      const basePage = bluetoothPairingRequestCodePage.shadowRoot.querySelector(
          'bluetooth-base-page');
      return basePage.buttonBarState.pair;
    };
    const deviceName = 'BeatsX';
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789',
        /*publicName=*/ deviceName,
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothPairingRequestCodePage.device = device.deviceProperties;
    await flushAsync();

    const message =
        bluetoothPairingRequestCodePage.shadowRoot.querySelector('#message');
    assertTrue(!!message);
    assertEquals(
        bluetoothPairingRequestCodePage.i18n('bluetoothEnterPin', deviceName),
        message.textContent.trim());

    // Test button states.
    assertEquals(ButtonState.DISABLED, getPairButtonState());
    assertTrue(!!getInput());

    getInput().value = '12345';
    await flushAsync();
    assertEquals(ButtonState.ENABLED, getPairButtonState());
  });
});