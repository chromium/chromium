// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_request_code_page.js';

import type {SettingsBluetoothRequestCodePageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_request_code_page.js';
import {ButtonState, PairingAuthType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

suite('CrComponentsBluetoothPairingRequestCodePageTest', function() {
  let bluetoothPairingRequestCodePage: SettingsBluetoothRequestCodePageElement;

  async function flushAsync(): Promise<null> {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothPairingRequestCodePage =
        document.createElement('bluetooth-pairing-request-code-page');
    document.body.appendChild(bluetoothPairingRequestCodePage);
    assertTrue(!!bluetoothPairingRequestCodePage);
    flush();
  });

  test(
      'Message, input length, button states and initial focus',
      async function() {
        const getInput = () => bluetoothPairingRequestCodePage.shadowRoot!
                                   .querySelector<CrInputElement>('#pin');
        const getPairButtonState = () => {
          const basePage =
              bluetoothPairingRequestCodePage.shadowRoot!.querySelector(
                  'bluetooth-base-page');
          return basePage!.buttonBarState.pair;
        };

        // Input should be focused by default.
        await waitAfterNextRender(bluetoothPairingRequestCodePage);
        assertEquals(
            getDeepActiveElement(),
            getInput()!.shadowRoot!.querySelector<HTMLInputElement>('input'));

        const deviceName = 'BeatsX';
        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456789',
            /*publicName=*/ deviceName,
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        bluetoothPairingRequestCodePage.device = device.deviceProperties;
        bluetoothPairingRequestCodePage.authType =
            PairingAuthType.REQUEST_PIN_CODE;
        await flushAsync();

        const input = getInput();
        assertTrue(!!input);
        // input uses ! flag because the compilar currently fails when
        // running test locally.
        assertEquals(input!.maxlength, 6);

        const message =
            bluetoothPairingRequestCodePage.shadowRoot!.querySelector(
                '#message');
        assertTrue(!!message);
        // message uses ! flag because the compilar currently fails when
        // running test locally.
        assertEquals(
            bluetoothPairingRequestCodePage.i18n(
                'bluetoothEnterPin', deviceName),
            message!.textContent!.trim());

        // Test button states.
        assertEquals(ButtonState.DISABLED, getPairButtonState());

        // input uses ! flag because the compilar currently fails when
        // running test locally.
        input!.value = '12345';
        await flushAsync();
        assertEquals(ButtonState.ENABLED, getPairButtonState());
      });

  test(
      'request-code-entered is fired when pair button is clicked',
      async function() {
        const requestCodePromise = eventToPromise(
            'request-code-entered', bluetoothPairingRequestCodePage);
        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456789',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        bluetoothPairingRequestCodePage.device = device.deviceProperties;
        await flushAsync();

        const basePage =
            bluetoothPairingRequestCodePage.shadowRoot!.querySelector(
                'bluetooth-base-page');
        assertTrue(!!basePage);
        const input = bluetoothPairingRequestCodePage.shadowRoot!
                          .querySelector<CrInputElement>('#pin');
        assertTrue(!!input);

        const pin = '12345';
        input!.value = pin;
        await flushAsync();

        assertTrue(!!basePage);
        // basePage uses ! flag because the compilar currently fails when
        // running test locally.
        basePage!.dispatchEvent(new CustomEvent('pair'));
        const requestCodeEvent = await requestCodePromise;
        assertEquals(requestCodeEvent.detail.code, pin);
      });
});
