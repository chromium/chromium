// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppVerifyPinDialogElement} from 'chrome://os-settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {hasStringProperty} from '../../utils.js';

suite('AppVerifyPinDialogTest', () => {
  let verifyPinDialog: AppVerifyPinDialogElement;

  setup(() => {
    verifyPinDialog = document.createElement('app-verify-pin-dialog');
    document.body.appendChild(verifyPinDialog);
  });

  teardown(() => {
    verifyPinDialog.remove();
  });

  test(
      `Submit button should be disabled if a PIN with the wrong length is entered`,
      async () => {
        const verifyPinKeyboard =
            verifyPinDialog.shadowRoot!.getElementById('pinKeyboard');

        assertTrue(!!verifyPinKeyboard);
        assertTrue(hasStringProperty(verifyPinKeyboard, 'value'));

        verifyPinKeyboard.value = '12345';
        await flushTasks();

        const verifyPinSubmitButton =
            verifyPinDialog.shadowRoot!.querySelector<HTMLElement>('#dialog')!
                .querySelector<HTMLButtonElement>('.action-button');

        assertTrue(!!verifyPinSubmitButton);
        assertTrue(verifyPinSubmitButton.disabled);
      });

  test(
      `Submit button should be enabled if a PIN with the right length is entered`,
      async () => {
        const verifyPinKeyboard =
            verifyPinDialog.shadowRoot!.getElementById('pinKeyboard');

        assertTrue(!!verifyPinKeyboard);
        assertTrue(hasStringProperty(verifyPinKeyboard, 'value'));

        verifyPinKeyboard.value = '123456';
        await flushTasks();

        const verifyPinSubmitButton =
            verifyPinDialog.shadowRoot!.querySelector<HTMLElement>('#dialog')!
                .querySelector<HTMLButtonElement>('.action-button');

        assertTrue(!!verifyPinSubmitButton);
        assertFalse(verifyPinSubmitButton.disabled);
      });
});
