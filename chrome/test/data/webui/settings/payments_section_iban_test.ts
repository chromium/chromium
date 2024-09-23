// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSimpleConfirmationDialogElement, CrInputElement, SettingsIbanEditDialogElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createIbanEntry} from './autofill_fake_data.js';
import {createPaymentsSection, getDefaultExpectations} from './payments_section_utils.js';

// clang-format on

/**
 * Helper function to update IBAN value in the IBAN field.
 */
async function updateIbanTextboxValue(
    valueInput: CrInputElement, value: string): Promise<void> {
  valueInput.focus();
  valueInput.value = value;
  await valueInput.updateComplete;
  valueInput.dispatchEvent(
      new CustomEvent('input', {bubbles: true, composed: true}));
}

/**
 * Helper function to wait for IBAN validation to complete and any associated UI
 * to be updated.
 */
async function ibanValidated(paymentsManager: TestPaymentsManager) {
  await paymentsManager.whenCalled('isValidIban');
  await microtasksFinished();
}

suite('PaymentsSectionIban', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
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
   * Returns an array containing all local and server IBAN items.
   */
  function getIbanListItems() {
    return document.body.querySelector('settings-payments-section')!.shadowRoot!
        .querySelector('#paymentsList')!.shadowRoot!.querySelectorAll(
            'settings-iban-list-entry');
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
        /*creditCards=*/[], /*ibans=*/[], {credit_card_enabled: {value: true}});
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
        /*creditCards=*/[], /*ibans=*/[], {credit_card_enabled: {value: true}});
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
        /*creditCards=*/[], [iban1, iban2], /*prefValues=*/ {});

    assertEquals(2, getIbanListItems().length);
  });

  test('verifyIbanSummarySublabelWithNickname', async function() {
    const iban = createIbanEntry('BA393385804800211234', 'My doctor\'s IBAN');

    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*prefValues=*/ {});

    assertEquals(1, getIbanListItems().length);

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
    await updateIbanTextboxValue(valueInput, 'FI1410093000123458');

    // Type in another valid IBAN value.
    await updateIbanTextboxValue(valueInput, 'IT60X0542811101000000123456');

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

  test('verifyIBANErrorMessage', async function() {
    // All IBANs in this test are invalid, but since we're using
    // TestPaymentsManager we have to set that explicitly.
    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    paymentsManager.setIsValidIban(false);

    // Creates an IBAN with empty value and nickname.
    const iban = createIbanEntry('', '');
    const ibanDialog = createIbanDialog(iban);

    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    // With an empty IBAN, the save button should be disabled but no error
    // should be shown.
    const saveButton = ibanDialog.$.saveButton;
    assertTrue(!!saveButton, 'Save button should be disabled for empty IBAN');
    const valueInput = ibanDialog.$.valueInput;
    assertFalse(
        valueInput.invalid, 'No error message should be shown for empty IBAN');

    // This invalid IBAN is of sufficient length that an error should be shown.
    await updateIbanTextboxValue(valueInput, 'IT60X0542811101000000123450');
    await ibanValidated(paymentsManager);

    assertTrue(
        !!saveButton,
        'Save button should be disabled for invalid IBAN >= 24 characters ' +
            'in length');
    assertTrue(
        valueInput.invalid,
        'Error message should be shown for invalid IBAN >= 24 characters ' +
            'in length');

    // This invalid IBAN is less than 24 characters. The save button should
    // remain disabled, but no error should be shown.
    await updateIbanTextboxValue(valueInput, 'FI1410093000123458');
    await ibanValidated(paymentsManager);

    assertTrue(
        !!saveButton,
        'Save button should be disabled for shorter invalid IBAN');
    assertFalse(
        valueInput.invalid,
        'No error message should be shown for shorter invalid IBAN while ' +
            'editing');

    // Now un-focus the field - this should trigger the error to show.
    valueInput.blur();
    await ibanValidated(paymentsManager);

    assertTrue(
        valueInput.invalid,
        'After unfocusing, an error message should be shown for shorter ' +
            'invalid IBAN');
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
        /*creditCards=*/[], [iban],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);

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
        /*creditCards=*/[], [iban], /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);

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
        /*creditCards=*/[], [iban], /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);

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

  test('verifyGooglePaymentsIndicatorAppearsForServerIbans', async function() {
    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);
    assertTrue(
        isVisible(getIbanRowShadowRoot(section.$.paymentsList)
                      .querySelector<HTMLElement>('#paymentsIndicator')));
  });

  test('verifyIbanRowButtonIsOutlinkForServerIbans', async function() {
    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const section = await createPaymentsSection(
        /*creditCards=*/[], [iban], /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);
    const rowShadowRoot = getIbanRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#ibanMenu');
    assertFalse(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
  });
});
