// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsPaymentsListElement} from 'chrome://settings/lazy_load.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry} from './autofill_fake_data.js';
// clang-format on

suite('PaymentsSectionPaymentsList', function() {
  let paymentsList: SettingsPaymentsListElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      showIbansSettings: true,
      shouldShowPayOverTimeSettings: true,
    });
  });

  async function createPaymentsList(
      creditCards: chrome.autofillPrivate.CreditCardEntry[],
      ibans: chrome.autofillPrivate.IbanEntry[],
      payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[]):
      Promise<SettingsPaymentsListElement> {
    const list = document.createElement('settings-payments-list');
    list.creditCards = creditCards;
    list.ibans = ibans;
    list.payOverTimeIssuers = payOverTimeIssuers;

    document.body.appendChild(list);
    await flushTasks();

    return list;
  }

  /**
   * Returns an array containing all payment method items from paymentsList.
   */
  function getPaymentsListPaymentMethodItems() {
    return paymentsList!.shadowRoot!.querySelectorAll<HTMLElement>(
        '.payment-method');
  }

  /**
   * Returns an array containing all list separator items from paymentsList.
   */
  function getPaymentsListListSeparatorItems() {
    return paymentsList!.shadowRoot!.querySelectorAll<HTMLElement>(
        '.list-separator');
  }

  test('verifyPaymentsListSeparatorsShown', async function() {
    paymentsList = await createPaymentsList(
        [createCreditCardEntry()], [createIbanEntry()],
        [createPayOverTimeIssuerEntry()]);

    const paymentMethods = getPaymentsListPaymentMethodItems();
    const listSeparators = getPaymentsListListSeparatorItems();

    assertEquals(3, paymentMethods.length);
    assertEquals(2, listSeparators.length);
    assertTrue(isVisible(listSeparators[0]!));
    assertTrue(isVisible(listSeparators[1]!));
  });

  test('verifyPaymentsListSeparatorsShownNoCreditCards', async function() {
    paymentsList = await createPaymentsList(
        /*creditCards=*/[], [createIbanEntry()],
        [createPayOverTimeIssuerEntry()]);

    const paymentMethods = getPaymentsListPaymentMethodItems();
    const listSeparators = getPaymentsListListSeparatorItems();

    assertEquals(2, paymentMethods.length);
    assertEquals(2, listSeparators.length);
    assertFalse(isVisible(listSeparators[0]!));
    assertTrue(isVisible(listSeparators[1]!));
  });

  test('verifyPaymentsListSeparatorsShownNoIbans', async function() {
    paymentsList = await createPaymentsList(
        [createCreditCardEntry()], /*ibans=*/[],
        [createPayOverTimeIssuerEntry()]);

    const paymentMethods = getPaymentsListPaymentMethodItems();
    const listSeparators = getPaymentsListListSeparatorItems();

    assertEquals(2, paymentMethods.length);
    assertEquals(2, listSeparators.length);
    assertFalse(isVisible(listSeparators[0]!));
    assertTrue(isVisible(listSeparators[1]!));
  });

  test('verifyPaymentsListSeparatorsShownNoIssuers', async function() {
    paymentsList = await createPaymentsList(
        [createCreditCardEntry()], [createIbanEntry()],
        /*payOverTimeIssuers=*/[]);

    const paymentMethods = getPaymentsListPaymentMethodItems();
    const listSeparators = getPaymentsListListSeparatorItems();

    assertEquals(2, paymentMethods.length);
    assertEquals(2, listSeparators.length);
    assertTrue(isVisible(listSeparators[0]!));
    assertFalse(isVisible(listSeparators[1]!));
  });

  test('verifyPaymentsListSeparatorsHiddenEmptyList', async function() {
    paymentsList = await createPaymentsList(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[]);

    const paymentMethods = getPaymentsListPaymentMethodItems();
    const listSeparators = getPaymentsListListSeparatorItems();

    assertEquals(0, paymentMethods.length);
    assertEquals(2, listSeparators.length);
    assertFalse(isVisible(listSeparators[0]!));
    assertFalse(isVisible(listSeparators[1]!));
  });

  test('verifyPaymentsListSeparatorsHiddenOnePaymentMethod', async function() {
    paymentsList = await createPaymentsList(
        /*creditCards=*/[], /*ibans=*/[], [createPayOverTimeIssuerEntry()]);

    const paymentMethods = getPaymentsListPaymentMethodItems();
    const listSeparators = getPaymentsListListSeparatorItems();

    assertEquals(1, paymentMethods.length);
    assertEquals(2, listSeparators.length);
    assertFalse(isVisible(listSeparators[0]!));
    assertFalse(isVisible(listSeparators[1]!));
  });

  test('verifyNoPaymentMethodsLabelShown', async function() {
    paymentsList = await createPaymentsList(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[]);

    assertEquals(0, getPaymentsListPaymentMethodItems().length);

    const noPaymentMethodsLabel =
        paymentsList.shadowRoot!.querySelector<HTMLElement>(
            '#noPaymentMethodsLabel');
    assertTrue(!!noPaymentMethodsLabel);
    assertTrue(isVisible(noPaymentMethodsLabel));
  });

  test('verifyNoPaymentMethodsLabelHidden', async function() {
    paymentsList = await createPaymentsList(
        [createCreditCardEntry()], [createIbanEntry()],
        [createPayOverTimeIssuerEntry()]);

    assertEquals(3, getPaymentsListPaymentMethodItems().length);

    const noPaymentMethodsLabel =
        paymentsList.shadowRoot!.querySelector<HTMLElement>(
            '#noPaymentMethodsLabel');
    assertTrue(!!noPaymentMethodsLabel);
    assertFalse(isVisible(noPaymentMethodsLabel));
  });

  test('verifyPaymentMethodsCount', async function() {
    const creditCards = [createCreditCardEntry()];
    const ibans = [createIbanEntry(), createIbanEntry()];
    const payOverTimeIssuers = [
      createPayOverTimeIssuerEntry(),
      createPayOverTimeIssuerEntry(),
      createPayOverTimeIssuerEntry(),
    ];

    paymentsList =
        await createPaymentsList(creditCards, ibans, payOverTimeIssuers);

    assertEquals(
        creditCards.length + ibans.length + payOverTimeIssuers.length,
        getPaymentsListPaymentMethodItems().length);
  });
});
