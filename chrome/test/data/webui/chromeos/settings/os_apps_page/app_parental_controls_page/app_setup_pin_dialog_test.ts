// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppSetupPinDialogElement, AppSetupPinKeyboardElement, ParentalControlsPinDialogError} from 'chrome://os-settings/lazy_load.js';
import {setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../../fake_metrics_private.js';
import {hasStringProperty} from '../../utils.js';

import {FakeAppParentalControlsHandler} from './fake_app_parental_controls_handler.js';

suite('AppSetupPinDialogElementTest', () => {
  let setupPinDialog: AppSetupPinDialogElement;
  let fakeMetricsPrivate: FakeMetricsPrivate;
  let handler: FakeAppParentalControlsHandler;

  function getSetupPinKeyboard(): AppSetupPinKeyboardElement {
    const setupPinKeyboard =
        setupPinDialog.shadowRoot!.querySelector<AppSetupPinKeyboardElement>(
          '#setupPinKeyboard');
    assertTrue(!!setupPinKeyboard);
    return setupPinKeyboard;
  }

  function getActionButton(): HTMLButtonElement {
    const actionButton =
        setupPinDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
    assertTrue(!!actionButton);
    return actionButton;
  }

  async function enterPin(pin: string): Promise<void> {
    const pinKeyboard =
        getSetupPinKeyboard().shadowRoot!.getElementById('pinKeyboard');
    assertTrue(!!pinKeyboard);
    assertTrue(hasStringProperty(pinKeyboard, 'value'));
    pinKeyboard.value = pin;
    await flushTasks();
  }

  setup(() => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);

    fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;
    setupPinDialog = document.createElement('app-setup-pin-dialog');
    document.body.appendChild(setupPinDialog);
  });

  teardown(() => {
    setupPinDialog.remove();
  });

  test('Writing a pin that is too long shows an error message', async () => {
    assertTrue(getActionButton().disabled);
    await enterPin('1234567');

    // Verify that error message is showing
    const problemDiv =
        getSetupPinKeyboard().shadowRoot!.getElementById('problemDiv');
    assertTrue(!!problemDiv);
    assertTrue(!!problemDiv.textContent);
    assertFalse(problemDiv.hasAttribute('invisible'));
    const errorMessage = problemDiv.textContent.trim();
    assertEquals(errorMessage, 'PIN must be 6 digits');
    assertTrue(getActionButton().disabled);
  });

  test('Writing a pin that is too short shows an error message', async () => {
    assertTrue(getActionButton().disabled);
    await enterPin('12345');

    // Verify that error message is showing
    const problemDiv =
        getSetupPinKeyboard().shadowRoot!.getElementById('problemDiv');
    assertTrue(!!problemDiv);
    assertTrue(!!problemDiv.textContent);
    assertFalse(problemDiv.hasAttribute('invisible'));
    const errorMessage = problemDiv.textContent.trim();
    assertEquals(errorMessage, 'PIN must be 6 digits');
    assertTrue(getActionButton().disabled);
  });

  test(
      'Writing a pin that contains non-digits shows an error message',
      async () => {
        assertTrue(getActionButton().disabled);
        await enterPin('1a3456');

        // Verify that error message is showing
        const problemDiv =
          getSetupPinKeyboard().shadowRoot!.getElementById('problemDiv');
        assertTrue(!!problemDiv);
        assertTrue(!!problemDiv.textContent);
        assertFalse(problemDiv.hasAttribute('invisible'));
        const errorMessage = problemDiv.textContent.trim();
        assertEquals(errorMessage, 'PIN must use numbers only');
        assertTrue(getActionButton().disabled);
      });

  test(
      'Writing a pin that meets the requirements shows no error message',
      async () => {
        assertTrue(getActionButton().disabled);
        await enterPin('123456');

        // Verify that error message is not showing
        const problemDiv =
        getSetupPinKeyboard().shadowRoot!.getElementById('problemDiv');
        assertTrue(!!problemDiv);
        assertTrue(!!problemDiv.textContent);
        assertTrue(problemDiv.hasAttribute('invisible'));
        const errorMessage = problemDiv.textContent.trim();
        assertEquals(errorMessage, '');
        assertFalse(getActionButton().disabled);
      });

  test(
      'Submitting a valid pin and clicking continue shows confirm step',
      async () => {
        const continueButton = getActionButton();
        assertTrue(continueButton.disabled);
        await enterPin('123456');

        assertFalse(continueButton.disabled);
        assertEquals(continueButton.textContent, 'Continue');
        continueButton.click();
        await flushTasks();

        const confirmButton = getActionButton();
        assertEquals(confirmButton.textContent, 'Confirm');
        assertTrue(confirmButton.disabled);
      });

  test('Submitting an invalid PIN records error to histogram', async () => {
    await enterPin('1234');

    getSetupPinKeyboard().doSubmit();
    await flushTasks();

    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'ChromeOS.OnDeviceControls.PinDialogError',
            ParentalControlsPinDialogError.INVALID_PIN_ON_SETUP));
  });
});
