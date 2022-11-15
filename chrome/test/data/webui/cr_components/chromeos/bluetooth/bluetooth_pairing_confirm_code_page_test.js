// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import './fake_bluetooth_config.js';

import {SettingsBluetoothPairingConfirmCodePageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_confirm_code_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';
import {eventToPromise} from '../../../chromeos/test_util.js';

suite('CrComponentsBluetoothPairingConfirmCodePageTest', function() {
  /** @type {?SettingsBluetoothPairingConfirmCodePageElement} */
  let deviceConfirmCodePage;

  setup(function() {
    deviceConfirmCodePage =
        /** @type {?SettingsBluetoothPairingConfirmCodePageElement} */ (
            document.createElement('bluetooth-pairing-confirm-code-page'));
    document.body.appendChild(deviceConfirmCodePage);
    assertTrue(!!deviceConfirmCodePage);
    flush();
  });


  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test(
      'Code is shown and confirm-code event is fired on pair click',
      async function() {
        const basePage = deviceConfirmCodePage.shadowRoot.querySelector(
            'bluetooth-base-page');
        assertTrue(!!basePage);
        const confirmCodePromise =
            eventToPromise('confirm-code', deviceConfirmCodePage);

        const code = '876542';
        const codeInput =
            deviceConfirmCodePage.shadowRoot.querySelector('#code');
        assertTrue(!!codeInput);

        deviceConfirmCodePage.code = code;
        await flushAsync();
        assertEquals(code, codeInput.textContent.trim());

        basePage.dispatchEvent(new CustomEvent('pair'));
        await confirmCodePromise;
      });
});
