// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppSetupPinDialogElement} from 'chrome://os-settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {hasStringProperty} from '../../utils.js';

suite('AppSetupPinKeyboardElementTest', () => {
  let setupPinDialog: AppSetupPinDialogElement;

  async function initPage(): Promise<void> {
    setupPinDialog = document.createElement('app-setup-pin-dialog');
    document.body.appendChild(setupPinDialog);
    await flushTasks();
  }

  teardown(() => {
    setupPinDialog.remove();
  });

  test(`Writing a pin that's too long shows an error message`, async () => {
    await initPage();

    const setupPinKeyboard =
        setupPinDialog.shadowRoot!.getElementById('setupPinKeyboard');
    assertTrue(!!setupPinKeyboard);

    const pinKeyboard =
        setupPinKeyboard.shadowRoot!.getElementById('pinKeyboard');

    assertTrue(!!pinKeyboard);
    assertTrue(hasStringProperty(pinKeyboard, 'value'));

    const pin = '1234567';
    pinKeyboard.value = pin;
    await flushTasks();

    // Verify that error message is showing
    const problemDiv =
        setupPinKeyboard.shadowRoot!.getElementById('problemDiv');
    assertTrue(!!problemDiv);
    assertTrue(!!problemDiv.textContent);
    const errorMessage = problemDiv.textContent.trim();

    assertEquals(errorMessage, 'PIN must be 6 digits');
    assertFalse(problemDiv.hasAttribute('invisible'));
  });

  test(`Writing a pin that's too short shows an error message`, async () => {
    await initPage();

    const setupPinKeyboard =
        setupPinDialog.shadowRoot!.getElementById('setupPinKeyboard');
    assertTrue(!!setupPinKeyboard);

    const pinKeyboard =
        setupPinKeyboard.shadowRoot!.getElementById('pinKeyboard');
    assertTrue(!!pinKeyboard);
    assertTrue(hasStringProperty(pinKeyboard, 'value'));

    const pin = '12345';
    pinKeyboard.value = pin;
    await flushTasks();

    // Verify that error message is showing
    const problemDiv =
        setupPinKeyboard.shadowRoot!.getElementById('problemDiv');
    assertTrue(!!problemDiv);
    assertTrue(!!problemDiv.textContent);
    const errorMessage = problemDiv.textContent.trim();

    assertEquals(errorMessage, 'PIN must be 6 digits');
    assertFalse(problemDiv.hasAttribute('invisible'));
  });

  test(
      `Writing a pin that contains non-digits shows an error message`,
      async () => {
        await initPage();

        const setupPinKeyboard =
            setupPinDialog.shadowRoot!.getElementById('setupPinKeyboard');
        assertTrue(!!setupPinKeyboard);

        const pinKeyboard =
            setupPinKeyboard.shadowRoot!.getElementById('pinKeyboard');
        assertTrue(!!pinKeyboard);
        assertTrue(hasStringProperty(pinKeyboard, 'value'));

        const pin = '1a3456';
        pinKeyboard.value = pin;
        await flushTasks();

        // Verify that error message is showing
        const problemDiv =
            setupPinKeyboard.shadowRoot!.getElementById('problemDiv');
        assertTrue(!!problemDiv);
        assertTrue(!!problemDiv.textContent);
        const errorMessage = problemDiv.textContent.trim();

        assertEquals(errorMessage, 'PIN must use numbers only');
        assertFalse(problemDiv.hasAttribute('invisible'));
      });

  test(
      `Writing a pin that meets the requirements shows no error message`,
      async () => {
        await initPage();

        const setupPinKeyboard =
            setupPinDialog.shadowRoot!.getElementById('setupPinKeyboard');
        assertTrue(!!setupPinKeyboard);

        const pinKeyboard =
            setupPinKeyboard.shadowRoot!.getElementById('pinKeyboard');
        assertTrue(!!pinKeyboard);
        assertTrue(hasStringProperty(pinKeyboard, 'value'));

        const pin = '123456';
        pinKeyboard.value = pin;
        await flushTasks();

        // Verify that error message is not showing
        const problemDiv =
            setupPinKeyboard.shadowRoot!.getElementById('problemDiv');
        assertTrue(!!problemDiv);
        assertTrue(!!problemDiv.textContent);
        const errorMessage = problemDiv.textContent.trim();

        assertTrue(problemDiv.hasAttribute('invisible'));
        assertEquals(errorMessage, '');
      });
});
