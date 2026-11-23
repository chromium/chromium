// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {CrInputElement, SettingsSimpleConfirmationDialogElement, SettingsCreditCardEditDialogElement, SettingsCreditCardEditDialogV2Element, SettingsVirtualCardUnenrollDialogElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, ProfileInfoBrowserProxyImpl} from 'chrome://settings/settings.js';
// <if expr="not is_chromeos">
import {SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
// </if>
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createCreditCardEntry, createEmptyCreditCardEntry} from './autofill_fake_data.js';
import {createPaymentsPage, getDefaultExpectations, getFirstCreditCardEntry, getLocalAndServerCreditCardListItems, setupPaymentsPrefs} from './payments_page_test_utils.js';
import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';
// <if expr="not is_chromeos">
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
// </if>

// clang-format on

suite('PaymentsPageCardDialogs', function() {
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
    });
    await setupPaymentsPrefs();
    prefService = PrefService.getInstance();
  });

  /**
   * Creates the Edit Credit Card dialog.
   */
  function createCreditCardDialog(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry):
      SettingsCreditCardEditDialogElement {
    return createCreditCardDialogWithPrefs(creditCardItem, {});
  }

  /**
   * Creates the Edit Credit Card dialog with prefs.
   */
  function createCreditCardDialogWithPrefs(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry,
      prefsValues: Record<string, {value: unknown}>):
      SettingsCreditCardEditDialogElement {
    for (const [key, pref] of Object.entries(prefsValues)) {
      prefService.setPrefValue(`autofill.${key}`, pref.value);
    }
    const dialog = document.createElement('settings-credit-card-edit-dialog');
    dialog.creditCard = creditCardItem;
    document.body.appendChild(dialog);
    return dialog;
  }

  /**
   * Creates a virtual card unenroll dialog.
   */
  function createVirtualCardUnenrollDialog(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry):
      SettingsVirtualCardUnenrollDialogElement {
    const dialog =
        document.createElement('settings-virtual-card-unenroll-dialog');
    dialog.creditCard = creditCardItem;
    document.body.appendChild(dialog);
    return dialog;
  }

  /**
   * Helper function to simulate input on a CrInput element.
   */
  async function simulateInput(
      inputElement: CrInputElement, input: string): Promise<void> {
    inputElement.focus();
    inputElement.value = input;
    await inputElement.updateComplete;
    inputElement.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
  }

  test('verifyAddVsEditCreditCardTitle', function() {
    const newCreditCard = createEmptyCreditCardEntry();
    const newCreditCardDialog = createCreditCardDialog(newCreditCard);
    const oldCreditCard = createCreditCardEntry();
    const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

    function getTitle(dialog: SettingsCreditCardEditDialogElement): string {
      return dialog.shadowRoot.querySelector('[slot=title]')!.textContent;
    }

    const oldTitle = getTitle(oldCreditCardDialog);
    const newTitle = getTitle(newCreditCardDialog);
    assertNotEquals(oldTitle, newTitle);
    assertNotEquals('', oldTitle);
    assertNotEquals('', newTitle);

    // Wait for dialogs to open before finishing test.
    return Promise.all([
      whenAttributeIs(newCreditCardDialog.$.dialog, 'open', ''),
      whenAttributeIs(oldCreditCardDialog.$.dialog, 'open', ''),
    ]);
  });

  test('verifyExpiredCreditCardYear', async function() {
    const creditCard = createCreditCardEntry();

    // 2015 is over unless time goes wobbly.
    const twentyFifteen = 2015;
    creditCard.expirationYear = twentyFifteen.toString();

    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const now = new Date();
    const maxYear = now.getFullYear() + 19;
    const yearInput = creditCardDialog.$.year;
    const yearOptions = yearInput.options;

    assertEquals('2015', yearOptions[0]!.textContent.trim());
    assertEquals(
        maxYear.toString(),
        yearOptions[yearOptions.length - 1]!.textContent.trim());
    assertEquals(creditCard.expirationYear, yearInput.value);
  });

  test('verifyVeryFutureCreditCardYear', async function() {
    const creditCard = createCreditCardEntry();

    // Expiring 25 years from now is unusual.
    const now = new Date();
    const farFutureYear = now.getFullYear() + 25;
    creditCard.expirationYear = farFutureYear.toString();

    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const yearInput = creditCardDialog.$.year;
    const yearOptions = yearInput.options;

    assertEquals(
        now.getFullYear().toString(), yearOptions[0]!.textContent.trim());
    assertEquals(
        farFutureYear.toString(),
        yearOptions[yearOptions.length - 1]!.textContent.trim());
    assertEquals(creditCard.expirationYear, yearInput.value);
  });

  test('verifyVeryNormalCreditCardYear', async function() {
    const creditCard = createCreditCardEntry();

    // Expiring 2 years from now is not unusual.
    const now = new Date();
    const nearFutureYear = now.getFullYear() + 2;
    creditCard.expirationYear = nearFutureYear.toString();
    const maxYear = now.getFullYear() + 19;

    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const yearInput = creditCardDialog.$.year;
    const yearOptions = yearInput.options;

    assertEquals(
        now.getFullYear().toString(), yearOptions[0]!.textContent.trim());
    assertEquals(
        maxYear.toString(),
        yearOptions[yearOptions.length - 1]!.textContent.trim());
    assertEquals(creditCard.expirationYear, yearInput.value);
  });

  test('verifySaveNewCreditCard', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });

    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialogWithPrefs(
        creditCard, {payment_cvc_storage: {value: true}});
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Not expired, but still can't be saved, because there's no card number.
    const expiredError = creditCardDialog.$.expiredError;
    assertEquals('hidden', getComputedStyle(expiredError).visibility);

    const saveButton = creditCardDialog.$.saveButton;
    assertTrue(saveButton.disabled);

    // Add a card number to enable saving.
    creditCardDialog.$.numberInput.value = '4444333322221111';
    await microtasksFinished();

    assertEquals('hidden', getComputedStyle(expiredError).visibility);
    assertFalse(saveButton.disabled);

    const cvcInput =
        creditCardDialog.shadowRoot.querySelector<HTMLInputElement>(
            '#cvcInput');
    assertTrue(!!cvcInput);
    assertTrue(isVisible(cvcInput));
    cvcInput.value = '123';
    await microtasksFinished();

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    saveButton.click();
    const event = await savedPromise;

    assertEquals(creditCard.guid, event.detail.guid);
    assertEquals(creditCard.cvc, event.detail.cvc);
  });

  test('verifyOnlyValidCardNumbersAllowed_ValidCases', async function() {
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput = creditCardDialog.$.numberInput;
    const saveButton = creditCardDialog.$.saveButton;

    // Taken from //components/autofill/core/browser/validation_unittest.cc
    const validCardNumbers = [
      '378282246310005',     '3714 4963 5398 431',  '3787-3449-3671-000',
      '5610591081018250',    '3056 9309 0259 04',   '3852-0000-0232-37',
      '6011111111111117',    '6011 0009 9013 9424', '3530-1113-3330-0000',
      '3566002020360505',
      '5555 5555 5555 4444',  // Mastercard.
      '5105-1051-0510-5100',
      '4111111111111111',  // Visa.
      '4012 8888 8888 1881', '4222-2222-2222-2',    '5019717010103742',
      '6331101999990016',    '6247130048162403',
      '4532261615476013542',  // Visa, 19 digits.
      '5067071446391278',     // Elo.
    ];
    for (const cardNumber of validCardNumbers) {
      // First set the input to something invalid, to reset the dialog.
      await simulateInput(numberInput, '0000000000000001');
      assertTrue(
          numberInput.invalid,
          'Precondition failed: numberInput should initially be invalid');
      assertTrue(
          saveButton.disabled,
          'Precondition failed: saveButton should initially be disabled');

      // Now check the test case.
      await simulateInput(numberInput, cardNumber);
      assertFalse(numberInput.invalid, `Expected ${cardNumber} to be valid`);
      assertFalse(
          saveButton.disabled,
          `Expected save button to be enabled for ${cardNumber}`);

      // Blur the input; the card should continue to be considered valid.
      numberInput.blur();
      await microtasksFinished();
      assertFalse(
          numberInput.invalid, `Expected ${cardNumber} to be valid after blur`);
      assertFalse(
          saveButton.disabled,
          `Expected save button to be enabled for ${cardNumber} after blur`);
    }
  });

  test(
      'verifyOnlyValidCardNumbersAllowed_InvalidCasesWithNoError',
      async function() {
        const creditCard = createCreditCardEntry();
        const creditCardDialog = createCreditCardDialog(creditCard);

        await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

        const numberInput = creditCardDialog.$.numberInput;
        const saveButton = creditCardDialog.$.saveButton;

        // These are numbers for which we should only disable the save button
        // but not show an error to the user. Partially taken from
        // //components/autofill/core/browser/validation_unittest.cc
        const invalidCardNumbers = [
          '4111 1111 112',       // passes a Luhn check but is < 12 characters
          '4111-1111-1111-123',  // fails a Luhn check but is < 16 characters
        ];
        for (const cardNumber of invalidCardNumbers) {
          // First set the input to something valid, to reset the dialog.
          await simulateInput(numberInput, '4444333322221111');
          assertFalse(
              numberInput.invalid,
              'Precondition failed: numberInput should initially be valid');
          assertFalse(
              saveButton.disabled,
              'Precondition failed: saveButton should initially be enabled');

          // Now check the test case.
          await simulateInput(numberInput, cardNumber);
          assertFalse(
              numberInput.invalid, `Expected ${cardNumber} to be valid`);
          assertTrue(
              saveButton.disabled,
              `Expected save button to be disabled for ${cardNumber}`);

          // Blur the input; this should do full verification and change the
          // card to be invalid.
          numberInput.blur();
          await microtasksFinished();
          assertTrue(
              numberInput.invalid,
              `Expected ${cardNumber} to be invalid after blur`);
          assertTrue(
              saveButton.disabled,
              `Expected save button to be disabled for ${
                  cardNumber} after blur`);
        }
      });

  test('verifyOnlyValidCardNumbersAllowed_InvalidCases', async function() {
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput = creditCardDialog.$.numberInput;
    const saveButton = creditCardDialog.$.saveButton;

    // These are numbers for which we should both disable the save button and
    // show an error to the user.
    // Partially taken from
    // //components/autofill/core/browser/validation_unittest.cc
    const invalidCardNumbers = [
      '41111111111111111115',  // passes a Luhn check but is too long
      '4111-1111-1111-1110',   // >= 16 characters and wrong Luhn checksum
      '3056 9309 0259 04aa',   // non-digit characters
    ];
    for (const cardNumber of invalidCardNumbers) {
      // First set the input to something valid, to reset the dialog.
      await simulateInput(numberInput, '4444333322221111');
      assertFalse(
          numberInput.invalid,
          'Precondition failed: numberInput should initially be valid');
      assertFalse(
          saveButton.disabled,
          'Precondition failed: saveButton should initially be enabled');

      // Now check the test case.
      await simulateInput(numberInput, cardNumber);
      assertTrue(numberInput.invalid, `Expected ${cardNumber} to be invalid`);
      assertTrue(
          saveButton.disabled,
          `Expected save button to be disabled for ${cardNumber}`);

      // Blur the input; the card number should remain invalid.
      numberInput.blur();
      await microtasksFinished();
      assertTrue(
          numberInput.invalid,
          `Expected ${cardNumber} to still be invalid after blur`);
      assertTrue(
          saveButton.disabled,
          `Expected save button to still be disabled for ${
              cardNumber} after blur`);
    }
  });

  test('verifyNotEditedEntryAfterCancel', async function() {
    const creditCard = createCreditCardEntry();
    let creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Edit a entry.
    creditCardDialog.$.nameInput.value = 'EditedName';
    creditCardDialog.$.nicknameInput.value = 'NickName';
    creditCardDialog.$.numberInput.value = '0000000000001234';
    await microtasksFinished();

    const cancelButton = creditCardDialog.$.cancelButton;
    cancelButton.click();

    await eventToPromise('close', creditCardDialog);

    creditCardDialog = createCreditCardDialog(creditCard);
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    assertEquals(creditCardDialog.$.nameInput.value, creditCard.name);
    assertEquals(creditCardDialog.$.numberInput.value, creditCard.cardNumber);
    assertEquals(
        creditCardDialog.$.nicknameInput.value, creditCard.nickname || '');
  });

  test('verifyNicknameCharacterCount', async function() {
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const charCount =
        creditCardDialog.shadowRoot.querySelector<HTMLElement>('#charCount');
    assertTrue(!!charCount);
    assertTrue(charCount.hidden);

    // It should be visible when nickname is present.
    const nicknameInput = creditCardDialog.$.nicknameInput;
    await simulateInput(nicknameInput, 'NickName');
    assertFalse(charCount.hidden);
    assertEquals('8/25', charCount.textContent.trim());

    // It should be hidden when nickname is empty.
    await simulateInput(nicknameInput, '');
    assertTrue(charCount.hidden);
  });

  test('verifyCancelCreditCardEdit', async function() {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    let saveFired = false;
    eventToPromise('save-credit-card', creditCardDialog).then(() => {
      saveFired = true;
    });

    const closePromise = eventToPromise('close', creditCardDialog);

    const cancelButton = creditCardDialog.$.cancelButton;
    cancelButton.click();

    await closePromise;

    // Wait a bit to ensure 'save-credit-card' is NOT fired.
    await new Promise(resolve => window.setTimeout(resolve, 100));
    assertFalse(saveFired);
  });

  test('verifyRemoveLocalCreditCardDialogConfirmed', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const page = await createPaymentsPage(
        [creditCard], /*ibans=*/[], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const firstEntry = getFirstCreditCardEntry(page.$.paymentsList);
    assertFalse(!!firstEntry.shadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        firstEntry.shadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    assertFalse(page.$.menuRemoveCreditCard.hidden);
    page.$.menuRemoveCreditCard.click();
    await microtasksFinished();

    const confirmationDialog =
        page.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#localCardDeleteConfirmDialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    const removeButton = confirmationDialog.$.confirm;
    removeButton.click();
    await microtasksFinished();

    // Wait for the dialog close event to propagate to the PaymentManager.
    await closePromise;

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.removedCreditCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyRemoveLocalCreditCardDialogCancelled', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const page = await createPaymentsPage(
        [creditCard], /*ibans=*/[], /*payOverTimeIssuers=*/[],
        /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const firstEntry = getFirstCreditCardEntry(page.$.paymentsList);
    assertFalse(!!firstEntry.shadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        firstEntry.shadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    assertFalse(page.$.menuRemoveCreditCard.hidden);
    page.$.menuRemoveCreditCard.click();
    await microtasksFinished();

    const confirmationDialog =
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    const cancelButton = confirmationDialog.$.cancel;
    cancelButton.click();
    await microtasksFinished();

    // Wait for the dialog close event to propagate to the PaymentManager.
    await closePromise;

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.removedCreditCards = 0;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyVirtualCardUnenrollDialogConfirmed', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.guid = '12345';
    const dialog = createVirtualCardUnenrollDialog(creditCard);

    // Wait for the dialog to open.
    await whenAttributeIs(dialog.$.dialog, 'open', '');

    const promise = eventToPromise('unenroll-virtual-card', dialog);
    dialog.$.confirmButton.click();
    const event = await promise;
    assertEquals(event.detail, '12345');
  });

  test('verifyVirtualCardUnenrollDialogContent', function() {
    const creditCard = createCreditCardEntry();
    const dialog = createVirtualCardUnenrollDialog(creditCard);

    const title = dialog.shadowRoot.querySelector('[slot=title]')!;
    const body = dialog.shadowRoot.querySelector('[slot=body]')!;
    assertNotEquals('', title.textContent);
    assertNotEquals('', body.textContent);

    // Wait for dialogs to open before finishing test.
    return whenAttributeIs(dialog.$.dialog, 'open', '');
  });

  [true, false].forEach((cvcStorageToggleEnabled) => {
    test(`verifyCvcInputVisible_${cvcStorageToggleEnabled}`, async function() {
      loadTimeData.overrideValues({
        cvcStorageAvailable: true,
      });
      const creditCard = createCreditCardEntry();
      const creditCardDialog = createCreditCardDialogWithPrefs(
          creditCard, {payment_cvc_storage: {value: cvcStorageToggleEnabled}});

      await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

      const cvcInput =
          creditCardDialog.shadowRoot.querySelector<HTMLInputElement>(
              '#cvcInput');
      assertEquals(cvcStorageToggleEnabled, !!cvcInput);
      assertEquals(cvcStorageToggleEnabled, isVisible(cvcInput));
    });
  });

  test('verifyCvcInputTitleAndPlaceholder', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialogWithPrefs(
        creditCard, {payment_cvc_storage: {value: true}});

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const cvcInput =
        creditCardDialog.shadowRoot.querySelector<CrInputElement>('#cvcInput');
    assertTrue(!!cvcInput);
    assertTrue(isVisible(cvcInput));

    const cvcInputTitle = cvcInput.$.label.textContent.trim();
    assertTrue(!!cvcInputTitle);
    assertEquals(
        loadTimeData.getString('creditCardCvcInputTitle'), cvcInputTitle);

    const cvcInputBoxPlaceholder = cvcInput.$.input.placeholder.trim();
    assertTrue(!!cvcInputBoxPlaceholder);
    assertEquals(
        loadTimeData.getString('creditCardCvcInputPlaceholder'),
        cvcInputBoxPlaceholder);
  });

  test('verifyCvcInputImageTitle', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialogWithPrefs(
        creditCard, {payment_cvc_storage: {value: true}});

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const cvcInputImage =
        creditCardDialog.shadowRoot.querySelector<HTMLImageElement>(
            '#cvcImage');
    assertTrue(!!cvcInputImage);
    assertEquals(
        loadTimeData.getString('creditCardCvcImageTitle'), cvcInputImage.title);

    const numberInput = creditCardDialog.$.numberInput;
    assertTrue(isVisible(numberInput));

    // AmEx card entry.
    await simulateInput(numberInput, '34');
    assertEquals(
        loadTimeData.getString('creditCardCvcAmexImageTitle'),
        cvcInputImage.title);

    // Non-AmEx card entry.
    await simulateInput(numberInput, '42');
    assertEquals(
        loadTimeData.getString('creditCardCvcImageTitle'), cvcInputImage.title);
  });
});

suite('PaymentsPageCardDialogsV2', function() {
  let prefService: PrefService;
  let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;
  // <if expr="not is_chromeos">
  let syncBrowserProxy: TestSyncBrowserProxy;
  // </if>

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
    });
    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);
    // <if expr="not is_chromeos">
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    // </if>
    await setupPaymentsPrefs();
    prefService = PrefService.getInstance();
  });

  /**
   * Creates the Edit Credit Card dialog.
   */
  function createCreditCardDialog(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry):
      SettingsCreditCardEditDialogV2Element {
    return createCreditCardDialogWithPrefs(creditCardItem, {});
  }

  /**
   * Creates the Edit Credit Card dialog with prefs.
   */
  function createCreditCardDialogWithPrefs(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry,
      prefsValues: Record<string, {value: unknown}>):
      SettingsCreditCardEditDialogV2Element {
    for (const [key, pref] of Object.entries(prefsValues)) {
      prefService.setPrefValue(`autofill.${key}`, pref.value);
    }
    const dialog =
        document.createElement('settings-credit-card-edit-dialog-v2');
    dialog.creditCard = creditCardItem;
    document.body.appendChild(dialog);
    return dialog;
  }

  /**
   * Helper function to simulate input on a CrInput element.
   */
  async function simulateInput(
      inputElement: CrInputElement, input: string): Promise<void> {
    inputElement.focus();
    inputElement.value = input;
    await inputElement.updateComplete;
    inputElement.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
  }

  test('verifyAddVsEditCreditCardTitle', function() {
    const newCreditCard = createEmptyCreditCardEntry();
    const newCreditCardDialog = createCreditCardDialog(newCreditCard);
    const oldCreditCard = createCreditCardEntry();
    const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

    function getTitle(dialog: SettingsCreditCardEditDialogV2Element): string {
      return dialog.shadowRoot.querySelector('#title > span')!.textContent;
    }

    const oldTitle = getTitle(oldCreditCardDialog);
    const newTitle = getTitle(newCreditCardDialog);
    assertNotEquals(oldTitle, newTitle);
    assertNotEquals('', oldTitle);
    assertNotEquals('', newTitle);

    // Wait for dialogs to open before finishing test.
    return Promise.all([
      whenAttributeIs(newCreditCardDialog.$.dialog, 'open', ''),
      whenAttributeIs(oldCreditCardDialog.$.dialog, 'open', ''),
    ]);
  });

  test('verifyAddCreditCardExpirationDate', async function() {
    const newCreditCard = createEmptyCreditCardEntry();
    const newDialog = createCreditCardDialog(newCreditCard);
    await whenAttributeIs(newDialog.$.dialog, 'open', '');

    // Adding a new card leaves expirationDateInput empty so the MM/YY
    // placeholder is shown, and saveButton starts disabled.
    assertEquals('', newDialog.$.expirationDateInput.value);
    assertFalse(newDialog.$.expirationDateInput.invalid);
    assertTrue(newDialog.$.saveButton.disabled);
  });

  test('verifyEditCreditCardExpirationDate', async function() {
    // Editing an existing card pre-populates MM/YY with a leading 0 on single
    // digit months and enables saveButton when the card is valid.
    const existingCard = createCreditCardEntry();
    existingCard.expirationMonth = '5';
    existingCard.expirationYear = '2099';
    const editDialog = createCreditCardDialog(existingCard);
    await whenAttributeIs(editDialog.$.dialog, 'open', '');

    assertEquals('05/99', editDialog.$.expirationDateInput.value);
    assertFalse(editDialog.$.expirationDateInput.invalid);
    assertFalse(editDialog.$.saveButton.disabled);
  });

  test('verifyExpirationDateFormattingAndValidation', async function() {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput = creditCardDialog.$.numberInput;
    const expirationInput = creditCardDialog.$.expirationDateInput;
    const saveButton = creditCardDialog.$.saveButton;

    await simulateInput(numberInput, '4444333322221111');
    assertTrue(saveButton.disabled);

    // Typing 2 digits automatically appends '/'. While still typing, no error
    // is shown, but Save remains disabled.
    await simulateInput(expirationInput, '06');
    assertEquals('06/', expirationInput.value);
    assertFalse(expirationInput.invalid);
    assertTrue(saveButton.disabled);

    // Blurring an incomplete expiration date marks it invalid with error.
    expirationInput.blur();
    await microtasksFinished();
    assertTrue(expirationInput.invalid);
    assertTrue(saveButton.disabled);

    // Invalid months (< 01 or > 12) immediately show an error even before blur.
    for (const invalidMonth of ['00', '13', '19/99']) {
      await simulateInput(expirationInput, invalidMonth);
      assertTrue(
          expirationInput.invalid,
          `Expected ${invalidMonth} to be marked invalid`);
      assertTrue(saveButton.disabled);
    }

    // Expired dates in the past are marked invalid.
    await simulateInput(expirationInput, '0120');
    assertEquals('01/20', expirationInput.value);
    assertTrue(expirationInput.invalid);
    assertTrue(saveButton.disabled);

    // Non-digit characters are stripped and input is capped at 4 digits
    // (MM/YY).
    await simulateInput(expirationInput, '12a/9987');
    assertEquals('12/99', expirationInput.value);
    assertFalse(expirationInput.invalid);
    assertFalse(saveButton.disabled);
  });

  test('verifySaveNewCreditCard', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });

    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialogWithPrefs(
        creditCard, {payment_cvc_storage: {value: true}});
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Can't be saved yet because there's no card number or expiration date.
    const saveButton = creditCardDialog.$.saveButton;
    assertTrue(saveButton.disabled);

    // Add a card number; still disabled until expiration date is entered.
    creditCardDialog.$.numberInput.value = '4444333322221111';
    await microtasksFinished();
    assertTrue(saveButton.disabled);

    // Add a valid expiration date to enable saving.
    creditCardDialog.$.expirationDateInput.value = '12/99';
    await microtasksFinished();
    assertFalse(saveButton.disabled);

    const cvcInput =
        creditCardDialog.shadowRoot.querySelector<HTMLInputElement>(
            '#cvcInput');
    assertTrue(!!cvcInput);
    assertTrue(isVisible(cvcInput));
    cvcInput.value = '123';
    await microtasksFinished();

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    saveButton.click();
    const event = await savedPromise;

    assertEquals(creditCard.guid, event.detail.guid);
    assertEquals('12', event.detail.expirationMonth);
    assertEquals('2099', event.detail.expirationYear);
    assertEquals(creditCard.cvc, event.detail.cvc);
  });

  test('verifyOnlyValidCardNumbersAllowed_ValidCases', async function() {
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput = creditCardDialog.$.numberInput;
    const saveButton = creditCardDialog.$.saveButton;

    // Taken from //components/autofill/core/browser/validation_unittest.cc
    const validCardNumbers = [
      '378282246310005',     '3714 4963 5398 431',  '3787-3449-3671-000',
      '5610591081018250',    '3056 9309 0259 04',   '3852-0000-0232-37',
      '6011111111111117',    '6011 0009 9013 9424', '3530-1113-3330-0000',
      '3566002020360505',
      '5555 5555 5555 4444',  // Mastercard.
      '5105-1051-0510-5100',
      '4111111111111111',  // Visa.
      '4012 8888 8888 1881', '4222-2222-2222-2',    '5019717010103742',
      '6331101999990016',    '6247130048162403',
      '4532261615476013542',  // Visa, 19 digits.
      '5067071446391278',     // Elo.
    ];
    for (const cardNumber of validCardNumbers) {
      // First set the input to something invalid, to reset the dialog.
      await simulateInput(numberInput, '0000000000000001');
      assertTrue(
          numberInput.invalid,
          'Precondition failed: numberInput should initially be invalid');
      assertTrue(
          saveButton.disabled,
          'Precondition failed: saveButton should initially be disabled');

      // Now check the test case.
      await simulateInput(numberInput, cardNumber);
      assertFalse(numberInput.invalid, `Expected ${cardNumber} to be valid`);
      assertFalse(
          saveButton.disabled,
          `Expected save button to be enabled for ${cardNumber}`);

      // Blur the input; the card should continue to be considered valid.
      numberInput.blur();
      await microtasksFinished();
      assertFalse(
          numberInput.invalid, `Expected ${cardNumber} to be valid after blur`);
      assertFalse(
          saveButton.disabled,
          `Expected save button to be enabled for ${cardNumber} after blur`);
    }
  });

  test(
      'verifyOnlyValidCardNumbersAllowed_InvalidCasesWithNoError',
      async function() {
        const creditCard = createCreditCardEntry();
        const creditCardDialog = createCreditCardDialog(creditCard);

        await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

        const numberInput = creditCardDialog.$.numberInput;
        const saveButton = creditCardDialog.$.saveButton;

        // These are numbers for which we should only disable the save button
        // but not show an error to the user. Partially taken from
        // //components/autofill/core/browser/validation_unittest.cc
        const invalidCardNumbers = [
          '4111 1111 112',       // passes a Luhn check but is < 12 characters
          '4111-1111-1111-123',  // fails a Luhn check but is < 16 characters
        ];
        for (const cardNumber of invalidCardNumbers) {
          // First set the input to something valid, to reset the dialog.
          await simulateInput(numberInput, '4444333322221111');
          assertFalse(
              numberInput.invalid,
              'Precondition failed: numberInput should initially be valid');
          assertFalse(
              saveButton.disabled,
              'Precondition failed: saveButton should initially be enabled');

          // Now check the test case.
          await simulateInput(numberInput, cardNumber);
          assertFalse(
              numberInput.invalid, `Expected ${cardNumber} to be valid`);
          assertTrue(
              saveButton.disabled,
              `Expected save button to be disabled for ${cardNumber}`);

          // Blur the input; this should do full verification and change the
          // card to be invalid.
          numberInput.blur();
          await microtasksFinished();
          assertTrue(
              numberInput.invalid,
              `Expected ${cardNumber} to be invalid after blur`);
          assertTrue(
              saveButton.disabled,
              `Expected save button to be disabled for ${
                  cardNumber} after blur`);
        }
      });

  test('verifyOnlyValidCardNumbersAllowed_InvalidCases', async function() {
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput = creditCardDialog.$.numberInput;
    const saveButton = creditCardDialog.$.saveButton;

    // These are numbers for which we should both disable the save button and
    // show an error to the user.
    // Partially taken from
    // //components/autofill/core/browser/validation_unittest.cc
    const invalidCardNumbers = [
      '41111111111111111115',  // passes a Luhn check but is too long
      '4111-1111-1111-1110',   // >= 16 characters and wrong Luhn checksum
      '3056 9309 0259 04aa',   // non-digit characters
    ];
    for (const cardNumber of invalidCardNumbers) {
      // First set the input to something valid, to reset the dialog.
      await simulateInput(numberInput, '4444333322221111');
      assertFalse(
          numberInput.invalid,
          'Precondition failed: numberInput should initially be valid');
      assertFalse(
          saveButton.disabled,
          'Precondition failed: saveButton should initially be enabled');

      // Now check the test case.
      await simulateInput(numberInput, cardNumber);
      assertTrue(numberInput.invalid, `Expected ${cardNumber} to be invalid`);
      assertTrue(
          saveButton.disabled,
          `Expected save button to be disabled for ${cardNumber}`);

      // Blur the input; the card number should remain invalid.
      numberInput.blur();
      await microtasksFinished();
      assertTrue(
          numberInput.invalid,
          `Expected ${cardNumber} to still be invalid after blur`);
      assertTrue(
          saveButton.disabled,
          `Expected save button to still be disabled for ${
              cardNumber} after blur`);
    }
  });

  test('verifyNotEditedEntryAfterCancel', async function() {
    const creditCard = createCreditCardEntry();
    let creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Edit a entry.
    creditCardDialog.$.nameInput.value = 'EditedName';
    creditCardDialog.$.nicknameInput.value = 'NickName';
    creditCardDialog.$.numberInput.value = '0000000000001234';
    await microtasksFinished();

    const cancelButton = creditCardDialog.$.cancelButton;
    cancelButton.click();

    await eventToPromise('close', creditCardDialog);

    creditCardDialog = createCreditCardDialog(creditCard);
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    assertEquals(creditCardDialog.$.nameInput.value, creditCard.name);
    assertEquals(creditCardDialog.$.numberInput.value, creditCard.cardNumber);
    assertEquals(
        creditCardDialog.$.nicknameInput.value, creditCard.nickname || '');
  });

  test('verifyNicknameCharacterCount', async function() {
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const charCount =
        creditCardDialog.shadowRoot.querySelector<HTMLElement>('#charCount');
    assertTrue(!!charCount);
    assertTrue(charCount.hidden);

    // It should be visible when nickname is present.
    const nicknameInput = creditCardDialog.$.nicknameInput;
    await simulateInput(nicknameInput, 'NickName');
    assertFalse(charCount.hidden);
    assertEquals('8/25', charCount.textContent.trim());

    // It should be hidden when nickname is empty.
    await simulateInput(nicknameInput, '');
    assertTrue(charCount.hidden);
  });

  test('verifyCancelCreditCardEdit', async function() {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    let saveFired = false;
    eventToPromise('save-credit-card', creditCardDialog).then(() => {
      saveFired = true;
    });

    const closePromise = eventToPromise('close', creditCardDialog);

    const cancelButton = creditCardDialog.$.cancelButton;
    cancelButton.click();

    await closePromise;

    // Wait a bit to ensure 'save-credit-card' is NOT fired.
    await new Promise(resolve => window.setTimeout(resolve, 100));
    assertFalse(saveFired);
  });

  [true, false].forEach((cvcStorageToggleEnabled) => {
    test(`verifyCvcInputVisible_${cvcStorageToggleEnabled}`, async function() {
      loadTimeData.overrideValues({
        cvcStorageAvailable: true,
      });
      const creditCard = createCreditCardEntry();
      const creditCardDialog = createCreditCardDialogWithPrefs(
          creditCard, {payment_cvc_storage: {value: cvcStorageToggleEnabled}});

      await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

      const cvcInput =
          creditCardDialog.shadowRoot.querySelector<HTMLInputElement>(
              '#cvcInput');
      assertEquals(cvcStorageToggleEnabled, !!cvcInput);
      assertEquals(cvcStorageToggleEnabled, isVisible(cvcInput));
    });
  });

  test('verifyCvcInputTitleAndPlaceholder', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });
    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialogWithPrefs(
        creditCard, {payment_cvc_storage: {value: true}});

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const cvcInput =
        creditCardDialog.shadowRoot.querySelector<CrInputElement>('#cvcInput');
    assertTrue(!!cvcInput);
    assertTrue(isVisible(cvcInput));

    const cvcInputTitle = cvcInput.$.label.textContent.trim();
    assertTrue(!!cvcInputTitle);
    assertEquals(
        loadTimeData.getString('creditCardCvcInputTitle'), cvcInputTitle);

    const cvcInputBoxPlaceholder = cvcInput.$.input.placeholder.trim();
    assertTrue(!!cvcInputBoxPlaceholder);
    assertEquals(
        loadTimeData.getString('creditCardCvcInputPlaceholder'),
        cvcInputBoxPlaceholder);
  });

  test('verifyCvcInputImageTitle', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialogWithPrefs(
        creditCard, {payment_cvc_storage: {value: true}});

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    const cvcInputImage =
        creditCardDialog.shadowRoot.querySelector<HTMLImageElement>(
            '#cvcImage');
    assertTrue(!!cvcInputImage);
    assertEquals(
        loadTimeData.getString('creditCardCvcImageTitle'), cvcInputImage.title);

    const numberInput = creditCardDialog.$.numberInput;
    assertTrue(isVisible(numberInput));

    // AmEx card entry.
    await simulateInput(numberInput, '34');
    assertEquals(
        loadTimeData.getString('creditCardCvcAmexImageTitle'),
        cvcInputImage.title);

    // Non-AmEx card entry.
    await simulateInput(numberInput, '42');
    assertEquals(
        loadTimeData.getString('creditCardCvcImageTitle'), cvcInputImage.title);
  });

  test('verifyUpdateOnProfileInfoChanged', async function() {
    const dialog = createCreditCardDialog(createEmptyCreditCardEntry());
    await whenAttributeIs(dialog.$.dialog, 'open', '');
    await profileInfoBrowserProxy.whenCalled('getProfileInfo');

    webUIListenerCallback(
        'profile-info-changed', {name: 'New Name', iconUrl: ''});
    await microtasksFinished();

    assertTrue(dialog.$.dialog.open);
    assertEquals(
        'New Name',
        dialog.shadowRoot.querySelector('#profile-name')!.textContent.trim());
  });

  // <if expr="not is_chromeos">
  test('verifyUpdateOnStoredAccountsUpdated', async function() {
    syncBrowserProxy.storedAccounts = [{
      fullName: 'Elisa Beckett',
      givenName: 'Elisa',
      email: 'elisa@gmail.com',
      avatarImage: '',
      isPrimaryAccount: true,
    }];
    const dialog = createCreditCardDialog(createEmptyCreditCardEntry());
    await whenAttributeIs(dialog.$.dialog, 'open', '');
    await syncBrowserProxy.whenCalled('getStoredAccounts');
    await microtasksFinished();

    assertEquals(
        'Elisa Beckett',
        dialog.shadowRoot.querySelector('#profile-name')!.textContent.trim());
    assertEquals(
        'elisa@gmail.com',
        dialog.shadowRoot.querySelector('#profile-email')!.textContent.trim());

    webUIListenerCallback('stored-accounts-updated', [{
                            fullName: 'Updated Name',
                            givenName: 'Updated',
                            email: 'updated@gmail.com',
                            avatarImage: '',
                            isPrimaryAccount: true,
                          }]);
    await microtasksFinished();

    assertTrue(dialog.$.dialog.open);
    assertEquals(
        'Updated Name',
        dialog.shadowRoot.querySelector('#profile-name')!.textContent.trim());
    assertEquals(
        'updated@gmail.com',
        dialog.shadowRoot.querySelector('#profile-email')!.textContent.trim());
    assertEquals(
        loadTimeData.getStringF(
            'paymentsSavedToGoogleAccount', 'updated@gmail.com'),
        dialog.shadowRoot.querySelector('#saved-to-google-account-label')!
            .textContent.trim());

    webUIListenerCallback('stored-accounts-updated', []);
    await microtasksFinished();

    assertTrue(dialog.$.dialog.open);
    assertEquals(
        '',
        dialog.shadowRoot.querySelector('#profile-email')!.textContent.trim());
    assertFalse(
        !!dialog.shadowRoot.querySelector('#saved-to-google-account-label'));
    assertTrue(
        !!dialog.shadowRoot.querySelector('#saved-to-this-device-only-label'));
  });

  test(
      'verifySavedToGoogleAccountLabelHiddenWithoutPrimaryAccount',
      async function() {
        syncBrowserProxy.storedAccounts = [];
        const dialog = createCreditCardDialog(createEmptyCreditCardEntry());
        await whenAttributeIs(dialog.$.dialog, 'open', '');
        await syncBrowserProxy.whenCalled('getStoredAccounts');
        await microtasksFinished();

        assertFalse(!!dialog.shadowRoot.querySelector(
            '#saved-to-google-account-label'));
        assertTrue(isVisible(dialog.shadowRoot.querySelector(
            '#saved-to-this-device-only-label')));
      });

  test(
      'verifySavedToGoogleAccountLabelShownWithPrimaryAccount',
      async function() {
        syncBrowserProxy.storedAccounts = [{
          fullName: 'Elisa Beckett',
          givenName: 'Elisa',
          email: 'elisa@gmail.com',
          avatarImage: '',
          isPrimaryAccount: true,
        }];
        const dialog = createCreditCardDialog(createEmptyCreditCardEntry());
        await whenAttributeIs(dialog.$.dialog, 'open', '');
        await syncBrowserProxy.whenCalled('getStoredAccounts');
        await microtasksFinished();

        assertFalse(!!dialog.shadowRoot.querySelector(
            '#saved-to-this-device-only-label'));
        const label = dialog.shadowRoot.querySelector<HTMLElement>(
            '#saved-to-google-account-label');
        assertTrue(!!label);
        assertTrue(isVisible(label));
        assertEquals(
            loadTimeData.getStringF(
                'paymentsSavedToGoogleAccount', 'elisa@gmail.com'),
            label.textContent.trim());
      });
  // </if>
});
