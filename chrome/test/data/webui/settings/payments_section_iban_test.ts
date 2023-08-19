// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsSimpleConfirmationDialogElement, CrInputElement, PaymentsManagerImpl, SettingsIbanEditDialogElement} from 'chrome://settings/lazy_load.js';
import {CrButtonElement, loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import {createIbanEntry, TestPaymentsManager} from './autofill_fake_data.js';
import {createPaymentsSection, getDefaultExpectations} from './payments_section_utils.js';

// clang-format on

/**
 * Helper function to update IBAN value in the IBAN field.
 */
function updateIbanTextboxValue(valueInput: CrInputElement, value: string) {
  valueInput.value = value;
  valueInput.dispatchEvent(
      new CustomEvent('input', {bubbles: true, composed: true}));
}


suite('PaymentsSectionIban', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      virtualCardEnrollmentEnabled: true,
      showIbansSettings: true,
    });
  });

  /**
   * Creates the Add or Edit IBAN dialog.
   */
  function createIbanDialog(ibanItem: chrome.autofillPrivate.IbanEntry):
      SettingsIbanEditDialogElement {
    const dialog = document.createElement('settings-iban-edit-dialog');
    dialog.iban = ibanItem;
    document.body.appendChild(dialog);
    flush();
    dialog.$.saveButton.disabled = false;
    return dialog;
  }

  /**
   * Returns an array containing all local IBAN items.
   */
  function getLocalIbanListItems() {
    return document.body.querySelector('settings-payments-section')!.shadowRoot!
        .querySelector('#paymentsList')!.shadowRoot!.querySelectorAll(
            'settings-iban-list-entry')!;
  }

  /**
   * Returns the shadow root of the IBAN row from the specified list of
   * payment methods.
   */
  function getIbanRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
    const row =
        paymentsList.shadowRoot!.querySelector('settings-iban-list-entry');
    assertTrue(!!row);
    return row.shadowRoot!;
  }

  test('verifyIbanSettingsDisabled', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: false,
    });
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    const addPaymentMethodsButton =
        section.shadowRoot!.querySelector<CrButtonElement>(
            '#addPaymentMethods');
    assertFalse(!!addPaymentMethodsButton);

    const addCreditCardButton =
        section.shadowRoot!.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCreditCardButton);
    assertFalse(addCreditCardButton.hidden);
  });

  test('verifyAddCardOrIbanPaymentMenu', async function() {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    const addPaymentMethodsButton =
        section.shadowRoot!.querySelector<CrButtonElement>(
            '#addPaymentMethods');
    assertTrue(!!addPaymentMethodsButton);
    addPaymentMethodsButton.click();
    flush();

    // "Add" menu should have 2 options.
    const addCreditCardButton =
        section.shadowRoot!.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCreditCardButton);
    assertFalse(addCreditCardButton.hidden);

    const addIbanButton =
        section.shadowRoot!.querySelector<CrButtonElement>('#addIban');
    assertTrue(!!addIbanButton);
    assertFalse(addIbanButton.hidden);
  });

  test('verifyListingAllLocalIBANs', async function() {
    const iban1 = createIbanEntry();
    const iban2 = createIbanEntry();
    await createPaymentsSection(
        /*creditCards=*/[], [iban1, iban2], /*upiIds=*/[], /*prefValues=*/ {});

    assertEquals(2, getLocalIbanListItems().length);
  });

  test('verifyIbanSummarySublabelWithNickname', async function() {
    const iban = createIbanEntry('BA393385804800211234', 'My doctor\'s IBAN');

    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*upiIds=*/[], /*prefValues=*/ {});

    assertEquals(1, getLocalIbanListItems().length);

    const ibanItemValue = getIbanRowShadowRoot(section.$.paymentsList)
                              .querySelector<HTMLElement>('#value');
    const ibanItemNickname = getIbanRowShadowRoot(section.$.paymentsList)
                                 .querySelector<HTMLElement>('#nickname');

    assertTrue(!!ibanItemValue);
    assertTrue(!!ibanItemNickname);
    assertEquals('BA39 **** **** **** 1234', ibanItemValue.textContent!.trim());
    assertEquals('My doctor\'s IBAN', ibanItemNickname.textContent!.trim());
  });

  test('verifySavingNewIBAN', async function() {
    // Creates an IBAN with empty value and nickname.
    const iban = createIbanEntry('', '');
    const ibanDialog = createIbanDialog(iban);

    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    const saveButton = ibanDialog.$.saveButton;
    assertTrue(!!saveButton);

    // Add a valid IBAN value.
    const valueInput = ibanDialog.$.valueInput;
    updateIbanTextboxValue(valueInput, 'FI1410093000123458');

    // Type in another valid IBAN value.
    updateIbanTextboxValue(valueInput, 'IT60X0542811101000000123456');

    const savePromise = eventToPromise('save-iban', ibanDialog);
    saveButton.click();
    const event = await savePromise;

    assertEquals(undefined, event.detail.guid);
    assertEquals('IT60X0542811101000000123456', event.detail.value);
    assertEquals('', event.detail.nickname);

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.isValidIban = 2;
    expectations.listeningCreditCards = 0;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyIbanEntryIsNotEditedAfterCancel', async function() {
    const iban = createIbanEntry('FI1410093000123458', 'NickName');
    let ibanDialog = createIbanDialog(iban);

    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    // Edit the value and nickname of the IBAN.
    const nicknameInput = ibanDialog.$.nicknameInput;
    nicknameInput.value = 'Updated NickName';

    const valueInput = ibanDialog.$.valueInput;
    valueInput.value = 'FI1410093000123412';
    flush();

    const cancelButton = ibanDialog.$.cancelButton;
    cancelButton.click();
    await eventToPromise('close', ibanDialog);

    ibanDialog = createIbanDialog(iban);
    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    assertEquals(ibanDialog.get('nickname_'), iban.nickname);
    assertEquals(ibanDialog.get('value_'), iban.value);
  });

  test('verifyLocalIbanMenu', async function() {
    const iban = createIbanEntry();
    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*upiIds=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getLocalIbanListItems().length);

    // Local IBANs will show the 3-dot overflow menu.
    section.$.ibanSharedActionMenu.get();
    const menuEditIban =
        section.shadowRoot!.querySelector<HTMLElement>('#menuEditIban');
    const menuRemoveIban =
        section.shadowRoot!.querySelector<HTMLElement>('#menuRemoveIban');

    // Menu should have 2 options.
    assertTrue(!!menuEditIban);
    assertTrue(!!menuRemoveIban);
    assertFalse(menuEditIban.hidden);
    assertFalse(menuRemoveIban!.hidden);

    flush();
  });

  test('verifyRemoveLocalIbanDialogConfirmed', async function() {
    const iban = createIbanEntry('FI1410093000123458', 'NickName');

    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalIbanListItems().length);

    const rowShadowRoot = getIbanRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot);
    const menuButton = rowShadowRoot.querySelector<HTMLElement>('#ibanMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    const menuRemoveIban =
        section.shadowRoot!.querySelector<CrButtonElement>('#menuRemoveIban');
    assertTrue(!!menuRemoveIban);
    assertFalse(menuRemoveIban.hidden);
    menuRemoveIban.click();
    flush();

    const confirmationDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#localIbanDeleteConfirmationDialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    confirmationDialog.$.confirm.click();
    flush();

    // Wait for the dialog close event to propagate to the PaymentManager.
    await closePromise;

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.removedIbans = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyRemoveLocalIbanDialogCancelled', async function() {
    const iban = createIbanEntry();

    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalIbanListItems().length);

    const rowShadowRoot = getIbanRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot);
    const menuButton = rowShadowRoot.querySelector<HTMLElement>('#ibanMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    const menuRemoveIban =
        section.shadowRoot!.querySelector<HTMLElement>('#menuRemoveIban');
    assertTrue(!!menuRemoveIban);
    menuRemoveIban.click();
    flush();

    const confirmationDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#localIbanDeleteConfirmationDialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    confirmationDialog.$.cancel.click();
    flush();

    const closePromise = eventToPromise('close', confirmationDialog);

    // Wait for the dialog close event to propagate to the PaymentManager.
    await closePromise;

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.removedIbans = 0;
    paymentsManager.assertExpectations(expectations);
  });
});
