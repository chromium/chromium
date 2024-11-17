// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import './fake_bluetooth_config.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_confirm_code_page.js';

import type {SettingsBluetoothPairingConfirmCodePageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_confirm_code_page.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.js';

suite('CrComponentsBluetoothPairingConfirmCodePageTest', function() {
  let deviceConfirmCodePage: SettingsBluetoothPairingConfirmCodePageElement;

  setup(async function() {
    deviceConfirmCodePage =
        document.createElement('bluetooth-pairing-confirm-code-page');
    document.body.appendChild(deviceConfirmCodePage);
    assertTrue(!!deviceConfirmCodePage);
    await flushTasks();
  });

  test(
      'Code is shown and confirm-code event is fired on pair click',
      async function() {
        const basePage = deviceConfirmCodePage.shadowRoot!.querySelector(
            'bluetooth-base-page');
        assertTrue(!!basePage);
        const confirmCodePromise =
            eventToPromise('confirm-code', deviceConfirmCodePage);

        const code = '876542';
        const codeInput =
            deviceConfirmCodePage.shadowRoot!.querySelector<HTMLDivElement>(
                '#code');
        assertTrue(!!codeInput);

        deviceConfirmCodePage.code = code;
        await flushTasks();
        assertEquals(code, codeInput!.textContent!.trim());

        basePage!.dispatchEvent(new CustomEvent('pair'));
        await confirmCodePromise;
      });
});
