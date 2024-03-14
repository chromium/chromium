// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsPaymentsSectionElement, SettingsCreditCardListEntryElement, SettingsIbanListEntryElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertTrue, assertLT} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';
// <if expr="is_win or is_macosx">
import {loadTimeData} from 'chrome://settings/settings.js';

// </if>

import {PaymentsManagerExpectations, TestPaymentsManager} from './autofill_fake_data.js';

// clang-format on

/**
 * Creates the payments autofill section for the given list.
 * @param {!Object} prefValues
 */
export async function createPaymentsSection(
    creditCards: chrome.autofillPrivate.CreditCardEntry[],
    ibans: chrome.autofillPrivate.IbanEntry[],
    prefValues: any): Promise<SettingsPaymentsSectionElement> {
  // Override the PaymentsManagerImpl for testing.
  const paymentsManager = new TestPaymentsManager();
  paymentsManager.data.creditCards = creditCards;
  paymentsManager.data.ibans = ibans;
  // <if expr="is_win or is_macosx">
  paymentsManager.setIsDeviceAuthAvailable(
      loadTimeData.getBoolean('deviceAuthAvailable'));
  // </if>
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
  expected.authenticateUserAndFlipMandatoryAuthToggle = 0;
  expected.getLocalCard = 0;
  expected.bulkDeleteAllCvcs = 0;
  return expected;
}

/**
 * Returns an array containing the local and server credit card items.
 */
export function getLocalAndServerCreditCardListItems() {
  return document.body.querySelector('settings-payments-section')!.shadowRoot!
      .querySelector('#paymentsList')!.shadowRoot!.querySelectorAll(
          'settings-credit-card-list-entry');
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
    section: SettingsPaymentsSectionElement, id: string): PaymentEntryElement {
  const container = section.$.paymentsList.shadowRoot;
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
    section: SettingsPaymentsSectionElement, type: PaymentMethod,
    index: number) {
  const id =
      type === PaymentMethod.CREDIT_CARD ? `card-${index}` : `iban-${index}`;
  const deleteButtonSelector = type === PaymentMethod.CREDIT_CARD ?
      '#menuRemoveCreditCard' :
      '#menuRemoveIban';

  // Open the dots menu:
  const entry = getPaymentMethodEntry(section, id);
  assertTrue(!!entry.dotsMenu);
  entry.dotsMenu.click();
  flush();

  // Click the Delete button:
  const deleteButton = section.shadowRoot!.querySelector<HTMLButtonElement>(
      deleteButtonSelector);
  assertTrue(!!deleteButton);
  deleteButton.click();
  flush();

  // Confirm the deletion in the dialog:
  const confirmationDialog =
      section.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
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
    section: SettingsPaymentsSectionElement, manager: TestPaymentsManager,
    type: PaymentMethod, index: number) {
  const deleteMethod =
      type === PaymentMethod.CREDIT_CARD ? 'removeCreditCard' : 'removeIban';
  const dataProperty =
      type === PaymentMethod.CREDIT_CARD ? 'creditCards' : 'ibans';

  // Ensure manager's deleteMethod call is caused by UI manipulations here.
  manager.resetResolver(deleteMethod);
  await executeUiManipulationsToDeletePaymentMethod(section, type, index);
  await manager.whenCalled(deleteMethod);

  // Create a copy to make sure all the Polymer updates get triggered.
  const paymentMethodItems = [...manager.data[dataProperty]];
  assertLT(index, paymentMethodItems.length);
  paymentMethodItems.splice(index, 1);
  manager.data[dataProperty] = paymentMethodItems;
  manager.lastCallback.setPersonalDataManagerListener!
      ([], manager.data.creditCards, manager.data.ibans);

  await flushTasks();
}
