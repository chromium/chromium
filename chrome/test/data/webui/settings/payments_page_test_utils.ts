// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsPaymentsPageElement, SettingsCreditCardListEntryElement, SettingsIbanListEntryElement, SettingsPaymentsListElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue, assertLT} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished, whenAttributeIs} from 'chrome://webui-test/test_util.js';
// <if expr="is_win or is_macosx">
import {loadTimeData} from 'chrome://settings/settings.js';

// </if>

import {PaymentsManagerExpectations, TestPaymentsManager} from './autofill_fake_data.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

export async function setupPaymentsPrefs(
    prefValues:
        Record<string, Partial<chrome.settingsPrivate.PrefObject>> = {}):
    Promise<TestPrefsBrowserProxy> {
  const prefs: chrome.settingsPrivate.PrefObject[] = [
    {
      key: 'autofill.credit_card_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
      ...prefValues['credit_card_enabled'],
    },
    {
      key: 'autofill.types_blocked',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [],
      ...prefValues['types_blocked'],
    },
    {
      key: 'autofill.payment_methods_mandatory_reauth',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
      ...prefValues['payment_methods_mandatory_reauth'],
    },
    {
      key: 'autofill.payment_cvc_storage',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
      ...prefValues['payment_cvc_storage'],
    },
    {
      key: 'autofill.payment_card_benefits',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
      ...prefValues['payment_card_benefits'],
    },
    {
      key: 'autofill.bnpl_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
      ...prefValues['bnpl_enabled'],
    },
    {
      key: 'payments.can_make_payment_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
      ...prefValues['can_make_payment_enabled'],
    },
    {
      key: 'signin.allowed_on_next_startup',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];
  const prefsBrowserProxy = new TestPrefsBrowserProxy(prefs);
  PrefsBrowserProxy.setInstance(prefsBrowserProxy);
  PrefService.resetInstanceForTesting();
  await PrefService.getInstance().whenInitialized();
  return prefsBrowserProxy;
}

/**
 * Creates the payments autofill page for the given list.
 * @param {!Object} prefValues
 */
export async function createPaymentsPage(
    creditCards: chrome.autofillPrivate.CreditCardEntry[],
    ibans: chrome.autofillPrivate.IbanEntry[],
    payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[],
    prefValues:
        Record<string, Partial<chrome.settingsPrivate.PrefObject>> = {}):
    Promise<SettingsPaymentsPageElement> {
  // Override the PaymentsManagerImpl for testing.
  const paymentsManager = new TestPaymentsManager();
  paymentsManager.data.creditCards = creditCards;
  paymentsManager.data.ibans = ibans;
  paymentsManager.data.payOverTimeIssuers = payOverTimeIssuers;
  // <if expr="is_win or is_macosx">
  paymentsManager.setIsDeviceAuthAvailable(
      loadTimeData.getBoolean('deviceAuthAvailable'));
  // </if>
  PaymentsManagerImpl.setInstance(paymentsManager);

  await setupPaymentsPrefs(prefValues);

  const page = document.createElement('settings-payments-page');
  document.body.appendChild(page);
  await microtasksFinished();

  return page;
}

/**
 * Returns the default expectations from TestPaymentsManager. Adjust the
 * values as needed.
 * `requestedCreditCards`, `listeningCreditCards` and `requestedIbans` are
 * defaulted to 1 as they are always called during page initialization.
 */
export function getDefaultExpectations(): PaymentsManagerExpectations {
  const expected = new PaymentsManagerExpectations();
  expected.requestedCreditCards = 1;
  expected.listeningCreditCards = 1;
  expected.removedCreditCards = 0;
  expected.addedVirtualCards = 0;
  expected.requestedIbans = 1;
  expected.removedIbans = 0;
  expected.isValidIban = 0;
  expected.requestedPayOverTimeIssuers = 0;
  expected.authenticateUserAndFlipMandatoryAuthToggle = 0;
  expected.getLocalCard = 0;
  expected.bulkDeleteAllCvcs = 0;
  return expected;
}

/**
 * Returns an array containing the local and server credit card items.
 */
export function getLocalAndServerCreditCardListItems() {
  return document.body.querySelector('settings-payments-page')!.shadowRoot
      .querySelector('settings-payments-list')!.shadowRoot.querySelectorAll(
          'settings-credit-card-list-entry');
}

/**
 * Returns the first credit card row from the specified list of payment methods.
 */
export function getFirstCreditCardEntry(
    paymentsList: SettingsPaymentsListElement):
    SettingsCreditCardListEntryElement {
  const row =
      paymentsList.shadowRoot.querySelector('settings-credit-card-list-entry');
  assertTrue(!!row);
  return row;
}

type PaymentEntryElement =
    SettingsCreditCardListEntryElement|SettingsIbanListEntryElement;

export const enum PaymentMethod {
  CREDIT_CARD,
  IBAN,
}

/**
 * Queries the payment method element by its DOM id.
 * See `SettingsPaymentsListElement` for the format of the ids.
 */
export function getPaymentMethodEntry(
    page: SettingsPaymentsPageElement, id: string): PaymentEntryElement {
  const container = page.$.paymentsList.shadowRoot;
  assertTrue(
      !!container,
      'the list element is expected to render its content in the shadowRoot');
  const element = container.querySelector<PaymentEntryElement>('#' + id);
  assertTrue(!!element, `payment method with DOM id ${id} is not found`);
  return element;
}
/**
 * The payment method is identified by the position (`index`) in the payment
 * method sub list (identified by the `type` argument).
 */
async function executeUiManipulationsToDeletePaymentMethod(
    page: SettingsPaymentsPageElement, type: PaymentMethod, index: number) {
  const id =
      type === PaymentMethod.CREDIT_CARD ? `card-${index}` : `iban-${index}`;
  const deleteButtonSelector = type === PaymentMethod.CREDIT_CARD ?
      '#menuRemoveCreditCard' :
      '#menuRemoveIban';

  // Open the dots menu:
  const entry = getPaymentMethodEntry(page, id);
  assertTrue(!!entry.dotsMenu);
  entry.dotsMenu.click();
  await microtasksFinished();

  // Click the Delete button:
  const deleteButton =
      page.shadowRoot.querySelector<HTMLButtonElement>(deleteButtonSelector);
  assertTrue(!!deleteButton);
  deleteButton.click();
  await microtasksFinished();

  // Confirm the deletion in the dialog:
  const confirmationDialog =
      page.shadowRoot.querySelector('settings-simple-confirmation-dialog');
  assertTrue(!!confirmationDialog);
  await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');
  const closePromise = eventToPromise('close', confirmationDialog.$.dialog);
  confirmationDialog.$.confirm.click();
  await closePromise;
}

/**
 * Performs required manipulations in the UI and manager to simulate the payment
 * method removal. The payment method is identified by the position (`index`) in
 * the respective (`type`) sub list.
 */
export async function deletePaymentMethod(
    page: SettingsPaymentsPageElement, manager: TestPaymentsManager,
    type: PaymentMethod, index: number) {
  const deleteMethod =
      type === PaymentMethod.CREDIT_CARD ? 'removeCreditCard' : 'removeIban';
  const dataProperty =
      type === PaymentMethod.CREDIT_CARD ? 'creditCards' : 'ibans';

  // Ensure manager's deleteMethod call is caused by UI manipulations here.
  manager.resetResolver(deleteMethod);
  await executeUiManipulationsToDeletePaymentMethod(page, type, index);
  await manager.whenCalled(deleteMethod);

  // Create a copy to make sure all the Polymer updates get triggered.
  const paymentMethodItems = [...manager.data[dataProperty]];
  assertLT(index, paymentMethodItems.length);
  paymentMethodItems.splice(index, 1);
  manager.data[dataProperty] = paymentMethodItems;
  manager.lastCallback.setPersonalDataManagerListener!
      ([], manager.data.creditCards, manager.data.ibans,
       manager.data.payOverTimeIssuers);

  await microtasksFinished();
}

/**
 * Verifies that a given boolean `histogramName` was recorded exactly once and
 * with the given `value`.
 */
export async function verifyBooleanHistogramRecorded(
    testMetricsBrowserProxy: TestMetricsBrowserProxy, histogramName: string,
    value: boolean) {
  const recordedHistograms =
      await testMetricsBrowserProxy.getArgs('recordBooleanHistogram');
  const filteredHistograms =
      recordedHistograms.filter(histogram => histogram[0] === histogramName);
  assertEquals(1, filteredHistograms.length);
  assertEquals(value, filteredHistograms[0][1]);
}

/**
 * Verifies that a given boolean `histogramName` was not recorded.
 */
export async function verifyBooleanHistogramNotRecorded(
    testMetricsBrowserProxy: TestMetricsBrowserProxy, histogramName: string) {
  const recordedHistograms =
      await testMetricsBrowserProxy.getArgs('recordBooleanHistogram');
  assertFalse(
      recordedHistograms.some(histogram => histogram[0] === histogramName));
}
