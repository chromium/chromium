// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenErrorElement} from 'chrome://personalization/js/personalization_app.js';
import {MantaStatusCode} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenErrorElementTest', function() {
  let seaPenErrorElement: SeaPenErrorElement|null;

  setup(() => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
  });

  teardown(async () => {
    await teardownElement(seaPenErrorElement);
    seaPenErrorElement = null;
  });

  test('display no network error state', async () => {
    seaPenErrorElement = initElement(
        SeaPenErrorElement,
        {thumbnailResponseStatusCode: MantaStatusCode.kNoInternetConnection});
    await waitAfterNextRender(seaPenErrorElement);

    const errorMessage = seaPenErrorElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage);
    assertEquals(
        seaPenErrorElement.i18n('seaPenErrorNoInternet'),
        errorMessage!.innerText);

    const errorIllo = seaPenErrorElement.shadowRoot!.querySelector(
                          'iron-icon') as HTMLElement;
    assertTrue(!!errorIllo);
    assertEquals(
        errorIllo.getAttribute('icon'),
        'personalization-shared-illo:network_error');
  });

  test('display resource exhausted error state', async () => {
    seaPenErrorElement = initElement(
        SeaPenErrorElement,
        {thumbnailResponseStatusCode: MantaStatusCode.kResourceExhausted});
    await waitAfterNextRender(seaPenErrorElement);

    const errorMessage = seaPenErrorElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage, 'an error message should be displayed');
    assertEquals(
        seaPenErrorElement.i18n('seaPenErrorResourceExhausted'),
        errorMessage!.innerText);

    const errorIllo = seaPenErrorElement.shadowRoot!.querySelector(
                          'iron-icon') as HTMLElement;
    assertTrue(!!errorIllo);
    assertEquals(
        errorIllo.getAttribute('icon'),
        'personalization-shared-illo:resource_error');
  });

  test('display user quota exceeded error state', async () => {
    seaPenErrorElement = initElement(
        SeaPenErrorElement,
        {thumbnailResponseStatusCode: MantaStatusCode.kPerUserQuotaExceeded});
    await waitAfterNextRender(seaPenErrorElement);

    const errorMessage = seaPenErrorElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage, 'an error message should be displayed');
    assertEquals(
        seaPenErrorElement.i18n('seaPenErrorResourceExhausted'),
        errorMessage!.innerText);

    const errorIllo = seaPenErrorElement.shadowRoot!.querySelector(
                          'iron-icon') as HTMLElement;
    assertTrue(!!errorIllo);
    assertEquals(
        errorIllo.getAttribute('icon'),
        'personalization-shared-illo:resource_error');
  });

  test('display generic error state', async () => {
    seaPenErrorElement = initElement(
        SeaPenErrorElement,
        {thumbnailResponseStatusCode: MantaStatusCode.kGenericError});
    await waitAfterNextRender(seaPenErrorElement);

    const errorMessage = seaPenErrorElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage, 'an error message should be displayed');
    assertEquals(
        seaPenErrorElement.i18n('seaPenErrorGeneric'), errorMessage!.innerText);

    const errorIllo = seaPenErrorElement.shadowRoot!.querySelector(
                          'iron-icon') as HTMLElement;
    assertTrue(!!errorIllo);
    assertEquals(
        errorIllo.getAttribute('icon'),
        'personalization-shared-illo:generic_error');
  });
});
