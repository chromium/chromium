// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PaymentsManagerImpl, SettingsPaymentsSectionElement} from 'chrome://settings/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {PaymentsManagerExpectations, TestPaymentsManager} from './passwords_and_autofill_fake_data.js';

// clang-format on

/**
 * Creates the payments autofill section for the given list.
 * @param {!Object} prefValues
 */
export async function createPaymentsSection(
    creditCards: chrome.autofillPrivate.CreditCardEntry[],
    ibans: chrome.autofillPrivate.IbanEntry[], upiIds: string[],
    prefValues: any): Promise<SettingsPaymentsSectionElement> {
  // Override the PaymentsManagerImpl for testing.
  const paymentsManager = new TestPaymentsManager();
  paymentsManager.data.creditCards = creditCards;
  paymentsManager.data.ibans = ibans;
  paymentsManager.data.upiIds = upiIds;
  PaymentsManagerImpl.setInstance(paymentsManager);

  const section = document.createElement('settings-payments-section');
  section.prefs = {autofill: prefValues};
  document.body.appendChild(section);
  await flushTasks();

  return section;
}

/**
 * Returns the default expectations from TestPaymentsManager. Adjust the
 * values as needed.
 */
export function getDefaultExpectations(): PaymentsManagerExpectations {
  const expected = new PaymentsManagerExpectations();
  expected.requestedCreditCards = 1;
  expected.listeningCreditCards = 1;
  expected.removedCreditCards = 0;
  expected.clearedCachedCreditCards = 0;
  expected.addedVirtualCards = 0;
  expected.requestedIbans = 1;
  expected.removedIbans = 0;
  expected.isValidIban = 0;
  return expected;
}

/**
 * Returns an array containing the local and server credit card items.
 */
export function getLocalAndServerCreditCardListItems() {
  return document.body.querySelector('settings-payments-section')!.shadowRoot!
      .querySelector('#paymentsList')!.shadowRoot!.querySelectorAll(
          'settings-credit-card-list-entry')!;
}

/**
 * Returns the shadow root of the card row from the specified list of
 * payment methods.
 */
export function getCardRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
  const row =
      paymentsList.shadowRoot!.querySelector('settings-credit-card-list-entry');
  assertTrue(!!row);
  return row.shadowRoot!;
}
