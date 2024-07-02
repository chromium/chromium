// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrInputElement, SettingsSimpleConfirmationDialogElement, SettingsCreditCardEditDialogElement, SettingsVirtualCardUnenrollDialogElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createCreditCardEntry, createEmptyCreditCardEntry} from './autofill_fake_data.js';
import {createPaymentsSection, getDefaultExpectations, getLocalAndServerCreditCardListItems, getCardRowShadowRoot} from './payments_section_utils.js';

// clang-format on

suite('PaymentsSectionCardDialogs', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
    });
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
      prefsValues: any): SettingsCreditCardEditDialogElement {
    const dialog = document.createElement('settings-credit-card-edit-dialog');
    dialog.creditCard = creditCardItem;
    dialog.prefs = {autofill: prefsValues};
    document.body.appendChild(dialog);
    flush();
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
    flush();
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
  }

  test('verifyAddVsEditCreditCardTitle', function() {
    const newCreditCard = createEmptyCreditCardEntry();
    const newCreditCardDialog = createCreditCardDialog(newCreditCard);
    const oldCreditCard = createCreditCardEntry();
    const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

    function getTitle(dialog: SettingsCreditCardEditDialogElement): string {
      return dialog.shadowRoot!.querySelector('[slot=title]')!.textContent!;
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

  test('verifyExpiredCreditCardYear', function() {
    const creditCard = createCreditCardEntry();

    // 2015 is over unless time goes wobbly.
    const twentyFifteen = 2015;
    creditCard.expirationYear = twentyFifteen.toString();

    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          const now = new Date();
          const maxYear = now.getFullYear() + 19;
          const yearInput =
              creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#year');
          const yearOptions = yearInput!.options;

          assertEquals('2015', yearOptions[0]!.textContent!.trim());
          assertEquals(
              maxYear.toString(),
              yearOptions[yearOptions.length - 1]!.textContent!.trim());
          assertEquals(creditCard.expirationYear, yearInput!.value);
        });
  });

  test('verifyVeryFutureCreditCardYear', function() {
    const creditCard = createCreditCardEntry();

    // Expiring 25 years from now is unusual.
    const now = new Date();
    const farFutureYear = now.getFullYear() + 25;
    creditCard.expirationYear = farFutureYear.toString();

    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          const yearInput =
              creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#year');
          const yearOptions = yearInput!.options;

          assertEquals(
              now.getFullYear().toString(),
              yearOptions[0]!.textContent!.trim());
          assertEquals(
              farFutureYear.toString(),
              yearOptions[yearOptions.length - 1]!.textContent!.trim());
          assertEquals(creditCard.expirationYear, yearInput!.value);
        });
  });

  test('verifyVeryNormalCreditCardYear', function() {
    const creditCard = createCreditCardEntry();

    // Expiring 2 years from now is not unusual.
    const now = new Date();
    const nearFutureYear = now.getFullYear() + 2;
    creditCard.expirationYear = nearFutureYear.toString();
    const maxYear = now.getFullYear() + 19;

    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          const yearInput =
              creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#year');
          const yearOptions = yearInput!.options;

          assertEquals(
              now.getFullYear().toString(),
              yearOptions[0]!.textContent!.trim());
          assertEquals(
              maxYear.toString(),
              yearOptions[yearOptions.length - 1]!.textContent!.trim());
          assertEquals(creditCard.expirationYear, yearInput!.value);
        });
  });

  [true, false].forEach((requireValidLocalCardsEnabled) => {
    const testSuffix = requireValidLocalCardsEnabled ?
        'requireValidLocalCards' :
        'doNotRequireValidLocalCards';
    test(`verifySaveNewCreditCard_${testSuffix}`, async function() {
      loadTimeData.overrideValues({
        cvcStorageAvailable: true,
        requireValidLocalCards: requireValidLocalCardsEnabled,
      });

      const creditCard = createEmptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialogWithPrefs(
          creditCard, {payment_cvc_storage: {value: true}});
      await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

      // Not expired, but still can't be saved, because there's no
      // name or card number.
      const expiredError =
          creditCardDialog.shadowRoot!.querySelector<HTMLElement>(
              '#expiredError');
      assertEquals('hidden', getComputedStyle(expiredError!).visibility);

      const saveButton =
          creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
              '#saveButton');
      assertTrue(saveButton!.disabled);

      if (requireValidLocalCardsEnabled) {
        // Add a card number to enable saving.
        creditCardDialog.set('rawCardNumber_', '4444333322221111');
      } else {
        // Add a card name to enable saving.
        creditCardDialog.set('name_', 'Jane Doe');
      }
      flush();

      assertEquals('hidden', getComputedStyle(expiredError!).visibility);
      assertFalse(saveButton!.disabled);

      const cvcInput =
          creditCardDialog.shadowRoot!.querySelector<HTMLInputElement>(
              '#cvcInput');
      assertTrue(!!cvcInput);
      assertTrue(isVisible(cvcInput));
      cvcInput.value = '123';

      const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
      saveButton!.click();
      const event = await savedPromise;

      assertEquals(creditCard.guid, event.detail.guid);
      assertEquals(creditCard.cvc, event.detail.cvc);
    });
  });

  test('verifyOnlyValidCardNumbersAllowed_ValidCases', async function() {
    loadTimeData.overrideValues({
      requireValidLocalCards: true,
    });

    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    assertTrue(!!numberInput, 'Precondition failed: numberInput should exist.');

    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
            '#saveButton');
    assertTrue(!!saveButton, 'Precondition failed: saveButton should exist.');

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
      flush();
      assertTrue(
          numberInput.invalid,
          'Precondition failed: numberInput should initially be invalid');
      assertTrue(
          saveButton!.disabled,
          'Precondition failed: saveButton should initially be disabled');

      // Now check the test case.
      await simulateInput(numberInput, cardNumber);
      flush();
      assertFalse(numberInput.invalid, `Expected ${cardNumber} to be valid`);
      assertFalse(
          saveButton!.disabled,
          `Expected save button to be enabled for ${cardNumber}`);

      // Blur the input; the card should continue to be considered valid.
      numberInput.blur();
      assertFalse(
          numberInput.invalid, `Expected ${cardNumber} to be valid after blur`);
      assertFalse(
          saveButton!.disabled,
          `Expected save button to be enabled for ${cardNumber} after blur`);
    }
  });

  test(
      'verifyOnlyValidCardNumbersAllowed_InvalidCasesWithNoError',
      async function() {
        loadTimeData.overrideValues({
          requireValidLocalCards: true,
        });

        const creditCard = createCreditCardEntry();
        const creditCardDialog = createCreditCardDialog(creditCard);

        await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

        const numberInput =
            creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
                '#numberInput');
        assertTrue(
            !!numberInput, 'Precondition failed: numberInput should exist.');

        const saveButton =
            creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
                '#saveButton');
        assertTrue(
            !!saveButton, 'Precondition failed: saveButton should exist.');

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
          flush();
          assertFalse(
              numberInput.invalid,
              'Precondition failed: numberInput should initially be valid');
          assertFalse(
              saveButton!.disabled,
              'Precondition failed: saveButton should initially be enabled');

          // Now check the test case.
          await simulateInput(numberInput, cardNumber);
          flush();
          assertFalse(
              numberInput.invalid, `Expected ${cardNumber} to be valid`);
          assertTrue(
              saveButton!.disabled,
              `Expected save button to be disabled for ${cardNumber}`);

          // Blur the input; this should do full verification and change the
          // card to be invalid.
          numberInput.blur();
          assertTrue(
              numberInput.invalid,
              `Expected ${cardNumber} to be invalid after blur`);
          assertTrue(
              saveButton!.disabled,
              `Expected save button to be disabled for ${
                  cardNumber} after blur`);
        }
      });

  test('verifyOnlyValidCardNumbersAllowed_InvalidCases', async function() {
    loadTimeData.overrideValues({
      requireValidLocalCards: true,
    });

    const creditCard = createCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    assertTrue(!!numberInput, 'Precondition failed: numberInput should exist.');

    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
            '#saveButton');
    assertTrue(!!saveButton, 'Precondition failed: saveButton should exist.');

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
      flush();
      assertFalse(
          numberInput.invalid,
          'Precondition failed: numberInput should initially be valid');
      assertFalse(
          saveButton!.disabled,
          'Precondition failed: saveButton should initially be enabled');

      // Now check the test case.
      await simulateInput(numberInput, cardNumber);
      flush();
      assertTrue(numberInput.invalid, `Expected ${cardNumber} to be invalid`);
      assertTrue(
          saveButton!.disabled,
          `Expected save button to be disabled for ${cardNumber}`);

      // Blur the input; the card number should remain invalid.
      numberInput.blur();
      assertTrue(
          numberInput.invalid,
          `Expected ${cardNumber} to still be invalid after blur`);
      assertTrue(
          saveButton!.disabled,
          `Expected save button to still be disabled for ${
              cardNumber} after blur`);
    }
  });

  test('verifyNotEditedEntryAfterCancel', async function() {
    const creditCard = createCreditCardEntry();
    let creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Edit a entry.
    creditCardDialog.set('name_', 'EditedName');
    creditCardDialog.set('nickname_', 'NickName');
    creditCardDialog.set('rawCardNumber_', '0000000000001234');
    flush();

    const cancelButton =
        creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
            '#cancelButton');
    cancelButton!.click();

    await eventToPromise('close', creditCardDialog);

    creditCardDialog = createCreditCardDialog(creditCard);
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    assertEquals(creditCardDialog.get('name_'), creditCard.name);
    assertEquals(creditCardDialog.get('rawCardNumber_'), creditCard.cardNumber);
    assertEquals(creditCardDialog.get('nickname_'), creditCard.nickname);
  });

  test('verifyCancelCreditCardEdit', function(done) {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    whenAttributeIs(creditCardDialog.$.dialog, 'open', '').then(function() {
      eventToPromise('save-credit-card', creditCardDialog).then(function() {
        // Fail the test because the save event should not be called
        // when cancel is clicked.
        assertTrue(false);
      });

      eventToPromise('close', creditCardDialog).then(function() {
        // Test is |done| in a timeout in order to ensure that
        // 'save-credit-card' is NOT fired after this test.
        window.setTimeout(done, 100);
      });

      const cancelButton =
          creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
              '#cancelButton');
      cancelButton!.click();
    });
  });

  test('verifyRemoveLocalCreditCardDialogConfirmed', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuRemoveCreditCard.hidden);
    section.$.menuRemoveCreditCard.click();
    flush();

    const confirmationDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#localCardDeleteConfirmDialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    const removeButton = confirmationDialog.$.confirm;
    assertTrue(!!removeButton);
    removeButton.click();
    flush();

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

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuRemoveCreditCard.hidden);
    section.$.menuRemoveCreditCard.click();
    flush();

    const confirmationDialog = section.shadowRoot!.querySelector(
        'settings-simple-confirmation-dialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    const cancelButton = confirmationDialog.$.cancel;
    assertTrue(!!cancelButton);
    cancelButton.click();
    flush();

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

    const title = dialog.shadowRoot!.querySelector('[slot=title]')!;
    const body = dialog.shadowRoot!.querySelector('[slot=body]')!;
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
          creditCardDialog.shadowRoot!.querySelector<HTMLInputElement>(
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
        creditCardDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#cvcInput');
    assertTrue(!!cvcInput);
    assertTrue(isVisible(cvcInput));

    const cvcInputTitle =
        cvcInput.shadowRoot!.querySelector<HTMLDivElement>(
                                '#label')!.textContent!.trim();
    assertTrue(!!cvcInputTitle);
    assertEquals(
        loadTimeData.getString('creditCardCvcInputTitle'), cvcInputTitle);

    const cvcInputBoxPlaceholder =
        cvcInput.shadowRoot!.querySelector<HTMLInputElement>(
                                '#input')!.placeholder!.trim();
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
        creditCardDialog.shadowRoot!.querySelector<HTMLImageElement>(
            '#cvcImage');
    assertTrue(!!cvcInputImage);
    assertEquals(
        loadTimeData.getString('creditCardCvcImageTitle'), cvcInputImage.title);

    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    assertTrue(!!numberInput);
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
