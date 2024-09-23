// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppVerifyPinDialogElement, ParentalControlsPinDialogError} from 'chrome://os-settings/lazy_load.js';
import {setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../../fake_metrics_private.js';
import {hasStringProperty} from '../../utils.js';

import {FakeAppParentalControlsHandler} from './fake_app_parental_controls_handler.js';

suite('AppVerifyPinDialogTest', () => {
  let verifyPinDialog: AppVerifyPinDialogElement;
  let fakeMetricsPrivate: FakeMetricsPrivate;
  let handler: FakeAppParentalControlsHandler;

  setup(() => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);

    fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    verifyPinDialog = document.createElement('app-verify-pin-dialog');
    verifyPinDialog.prefs = {
      on_device_app_controls: {
        pin: {
          value: '123456',
        },
      },
    };
    document.body.appendChild(verifyPinDialog);
  });

  teardown(() => {
    verifyPinDialog.remove();
  });

  test(
      'Incorrect PIN length disables submit button', async () => {
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
      'Correct PIN length enables submit button', async () => {
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

  test('Submitting incorrect PIN records error to histogram', async () => {
    const verifyPinKeyboard =
        verifyPinDialog.shadowRoot!.getElementById('pinKeyboard');

    assertTrue(!!verifyPinKeyboard);
    assertTrue(hasStringProperty(verifyPinKeyboard, 'value'));

    verifyPinKeyboard.value = '123123';
    await flushTasks();
    const verifyPinSubmitButton =
        verifyPinDialog.shadowRoot!.querySelector<HTMLElement>('#dialog')!
            .querySelector<HTMLButtonElement>('.action-button');

    assertTrue(!!verifyPinSubmitButton);
    verifyPinSubmitButton.click();
    await flushTasks();

    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'ChromeOS.OnDeviceControls.PinDialogError',
            ParentalControlsPinDialogError.INCORRECT_PIN));
  });

  test('Clicking forgot PIN records error to histogram', async () => {
    const forgotPinLink =
        verifyPinDialog.shadowRoot!.querySelector<HTMLElement>(
            '#forgotPinLink');
    assertTrue(!!forgotPinLink);
    forgotPinLink.click();
    await flushTasks();

    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'ChromeOS.OnDeviceControls.PinDialogError',
            ParentalControlsPinDialogError.FORGOT_PIN));
  });
});
