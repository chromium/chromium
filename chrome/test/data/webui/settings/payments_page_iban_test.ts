// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsSimpleConfirmationDialogElement, CrInputElement, SettingsIbanEditDialogElement, SettingsIbanListEntryElement, SettingsPaymentsListElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createIbanEntry} from './autofill_fake_data.js';
import {createPaymentsPage, getDefaultExpectations, setupPaymentsPrefs} from './payments_page_test_utils.js';

// clang-format on

/**
 * Helper function to update IBAN value in the IBAN field.
 */
async function updateIbanTextboxValue(
    valueInput: CrInputElement, value: string): Promise<void> {
  valueInput.focus();
  valueInput.value = value;
  await microtasksFinished();
  valueInput.fire('input');
}

/**
 * Helper function to wait for IBAN validation to complete and any associated UI
 * to be updated.
 */
async function ibanValidated(paymentsManager: TestPaymentsManager) {
  await paymentsManager.whenCalled('isValidIban');
  await microtasksFinished();
}

suite('PaymentsPageIban', function() {
  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
      autofillEnableWalletBranding: true,
      autofillEnableGradientGoogleLogos: false,
    });
    await setupPaymentsPrefs();
  });

  /**
   * Creates the Add or Edit IBAN dialog.
   */
  function createIbanDialog(ibanItem: chrome.autofillPrivate.IbanEntry):
      SettingsIbanEditDialogElement {
    const dialog = document.createElement('settings-iban-edit-dialog');
    dialog.iban = ibanItem;
    document.body.appendChild(dialog);
    dialog.$.saveButton.disabled = false;
    return dialog;
  }

  /**
   * Returns an array containing all local and server IBAN items.
   */
  function getIbanListItems() {
    return document.body.querySelector('settings-payments-page')!.shadowRoot
        .querySelector('settings-payments-list')!.shadowRoot.querySelectorAll(
            'settings-iban-list-entry');
  }

  /**
   * Returns the first IBAN row from the specified list of payment methods.
   */
  function getFirstIbanEntry(paymentsList: SettingsPaymentsListElement):
      SettingsIbanListEntryElement {
    const row =
        paymentsList.shadowRoot.querySelector('settings-iban-list-entry');
    assertTrue(!!row);
    return row;
  }

  test('verifyIbanSettingsDisabled', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: false,
    });
    const page = await createPaymentsPage(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[],
        {credit_card_enabled: {value: true}});
    const addPaymentMethodsButton =
        page.shadowRoot.querySelector<CrButtonElement>('#addPaymentMethods');
    assertFalse(!!addPaymentMethodsButton);

    const addCreditCardButton =
        page.shadowRoot.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCreditCardButton);
    assertFalse(addCreditCardButton.hidden);
  });

  test('verifyAddCardOrIbanPaymentMenu', async function() {
    const page = await createPaymentsPage(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[],
        {credit_card_enabled: {value: true}});
    const addPaymentMethodsButton =
        page.shadowRoot.querySelector<CrButtonElement>('#addPaymentMethods');
    assertTrue(!!addPaymentMethodsButton);
    addPaymentMethodsButton.click();
    await microtasksFinished();

    // "Add" menu should have 2 options.
    const addCreditCardButton =
        page.shadowRoot.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCreditCardButton);
    assertFalse(addCreditCardButton.hidden);

    const addIbanButton =
        page.shadowRoot.querySelector<CrButtonElement>('#addIban');
    assertTrue(!!addIbanButton);
    assertFalse(addIbanButton.hidden);
  });

  test('verifyListingAllLocalIBANs', async function() {
    const iban1 = createIbanEntry();
    const iban2 = createIbanEntry();
    await createPaymentsPage(
        /*creditCards=*/[], [iban1, iban2], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});

    assertEquals(2, getIbanListItems().length);
  });

  test('verifyIbanSummarySublabelWithNickname', async function() {
    const iban = createIbanEntry('BA393385804800211234', 'My doctor\'s IBAN');

    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});

    assertEquals(1, getIbanListItems().length);

    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const ibanItemLabel =
        ibanEntry.shadowRoot.querySelector<HTMLElement>('#label');
    const ibanItemSubLabel =
        ibanEntry.shadowRoot.querySelector<HTMLElement>('#subLabel');

    assertTrue(!!ibanItemLabel);
    assertTrue(!!ibanItemSubLabel);
    assertEquals('My doctor\'s IBAN', ibanItemLabel.textContent.trim());
    assertEquals(
        'BA39 **** **** **** 1234', ibanItemSubLabel.textContent.trim());
  });

  test('verifyNicknameCharacterCount', async function() {
    const iban = createIbanEntry('', '');
    const ibanDialog = createIbanDialog(iban);

    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    const charCount =
        ibanDialog.shadowRoot.querySelector<HTMLElement>('#charCount');
    assertTrue(!!charCount);
    assertTrue(charCount.hidden);

    // It should be visible when nickname is present.
    const nicknameInput = ibanDialog.$.nicknameInput;
    await updateIbanTextboxValue(nicknameInput, 'NickName');
    assertFalse(charCount.hidden);
    assertEquals('8/25', charCount.textContent.trim());

    // It should be hidden when nickname is empty.
    await updateIbanTextboxValue(nicknameInput, '');
    assertTrue(charCount.hidden);
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
    await microtasksFinished();

    const cancelButton = ibanDialog.$.cancelButton;
    cancelButton.click();
    await eventToPromise('close', ibanDialog);

    ibanDialog = createIbanDialog(iban);
    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    assertEquals(ibanDialog.$.nicknameInput.value, iban.nickname);
    assertEquals(ibanDialog.$.valueInput.value, iban.value);
  });

  test('verifyLocalIbanMenu', async function() {
    const iban = createIbanEntry();
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);

    // Local IBANs will show the 3-dot overflow menu.
    page.$.ibanSharedActionMenu.get();
    const menuEditIban =
        page.shadowRoot.querySelector<HTMLElement>('#menuEditIban');
    const menuRemoveIban =
        page.shadowRoot.querySelector<HTMLElement>('#menuRemoveIban');

    // Menu should have 2 options.
    assertTrue(!!menuEditIban);
    assertTrue(!!menuRemoveIban);
    assertFalse(menuEditIban.hidden);
    assertFalse(menuRemoveIban.hidden);

    await microtasksFinished();
  });

  test('verifyRemoveLocalIbanDialogConfirmed', async function() {
    const iban = createIbanEntry('FI1410093000123458', 'NickName');

    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);

    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const menuButton =
        ibanEntry.shadowRoot.querySelector<HTMLElement>('#ibanMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    const menuRemoveIban =
        page.shadowRoot.querySelector<CrButtonElement>('#menuRemoveIban');
    assertTrue(!!menuRemoveIban);
    assertFalse(menuRemoveIban.hidden);
    menuRemoveIban.click();
    await microtasksFinished();

    const confirmationDialog =
        page.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#localIbanDeleteConfirmationDialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    confirmationDialog.$.confirm.click();
    await microtasksFinished();

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

    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);

    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const menuButton =
        ibanEntry.shadowRoot.querySelector<HTMLElement>('#ibanMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    const menuRemoveIban =
        page.shadowRoot.querySelector<HTMLElement>('#menuRemoveIban');
    assertTrue(!!menuRemoveIban);
    menuRemoveIban.click();
    await microtasksFinished();

    const confirmationDialog =
        page.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#localIbanDeleteConfirmationDialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    confirmationDialog.$.cancel.click();
    await microtasksFinished();

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
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);
    assertTrue(isVisible(
        getFirstIbanEntry(page.$.paymentsList)
            .shadowRoot.querySelector<HTMLElement>('#paymentsIndicator')));
  });

  test('verifyIbanRowButtonIsOutlinkForServerIbans', async function() {
    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);
    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const menuButton = ibanEntry.shadowRoot.querySelector('#ibanMenu');
    assertFalse(!!menuButton);
    const outlinkButton =
        ibanEntry.shadowRoot.querySelector('cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
  });

  test('verifyIbanGooglePayOutlinkText', async function() {
    loadTimeData.overrideValues({
      autofillEnableWalletBranding: false,
    });

    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);
    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const outlinkButton = ibanEntry.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);

    assertEquals('Your payment methods in Google Pay', outlinkButton.title);
  });

  test('verifyIbanGoogleWalletOutlinkText', async function() {
    loadTimeData.overrideValues({
      autofillEnableWalletBranding: true,
    });

    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getIbanListItems().length);
    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const outlinkButton = ibanEntry.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);

    assertEquals('Your payment methods in Google Wallet', outlinkButton.title);
  });

  test('verifyGooglePayLogoWithGradient', async function() {
    loadTimeData.overrideValues({
      autofillEnableGradientGoogleLogos: true,
    });
    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const paymentsIcon = ibanEntry.shadowRoot.querySelector('#paymentsIcon');
    // #paymentsIcon is only present in Google Chrome branded builds.
    if (paymentsIcon) {
      const source = paymentsIcon.querySelector('source');
      const img = paymentsIcon.querySelector('img');
      assertTrue(!!source);
      assertTrue(!!img);
      assertTrue(source.srcset.includes(
          'IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_DARK_SMALL'));
      assertTrue(
          img.srcset.includes('IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_SMALL'));
    } else {
      const textIndicator =
          ibanEntry.shadowRoot.querySelector('#paymentsIndicator .sub-label');
      assertTrue(!!textIndicator);
      assertTrue(isVisible(textIndicator));
    }
  });

  test('verifyGooglePayLogoWithoutGradient', async function() {
    loadTimeData.overrideValues({
      autofillEnableGradientGoogleLogos: false,
    });
    const iban = createIbanEntry();
    iban.metadata!.isLocal = false;
    const page = await createPaymentsPage(
        /*creditCards=*/[], [iban], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    const ibanEntry = getFirstIbanEntry(page.$.paymentsList);
    const paymentsIcon = ibanEntry.shadowRoot.querySelector('#paymentsIcon');
    // #paymentsIcon is only present in Google Chrome branded builds.
    if (paymentsIcon) {
      const source = paymentsIcon.querySelector('source');
      const img = paymentsIcon.querySelector('img');
      assertTrue(!!source);
      assertTrue(!!img);
      assertTrue(source.srcset.includes('IDR_AUTOFILL_GOOGLE_PAY_DARK_SMALL'));
      assertTrue(img.srcset.includes('IDR_AUTOFILL_GOOGLE_PAY_SMALL'));
    } else {
      const textIndicator =
          ibanEntry.shadowRoot.querySelector('#paymentsIndicator .sub-label');
      assertTrue(!!textIndicator);
      assertTrue(isVisible(textIndicator));
    }
  });
});
