// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import type {CrInputElement, SettingsCreditCardEditDialogElement, SettingsIbanEditDialogElement, SettingsPaymentsSectionElement} from 'chrome://settings/lazy_load.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished, whenAttributeIs} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createCreditCardEntry, createIbanEntry, TestPaymentsManager} from './autofill_fake_data.js';
// clang-format on

/**
 * Helper function to simulate typing in nickname in the nickname field.
 */
async function typeInNickname(
    nicknameInput: CrInputElement, nickname: string): Promise<void> {
  nicknameInput.value = nickname;
  await nicknameInput.updateComplete;
  nicknameInput.dispatchEvent(
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

suite('PaymentsSectionCreditCardEditDialogTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      showIbansSettings: true,
      cvcStorageAvailable: true,
    });
  });

  /**
   * Creates the payments section for the given credit card and IBAN list.
   */
  async function createPaymentsSection(
      creditCards: chrome.autofillPrivate.CreditCardEntry[],
      ibans: chrome.autofillPrivate.IbanEntry[]):
      Promise<SettingsPaymentsSectionElement> {
    // Override the PaymentsManagerImpl for testing.
    const paymentsManager = new TestPaymentsManager();
    paymentsManager.data.creditCards = creditCards;
    paymentsManager.data.ibans = ibans;
    PaymentsManagerImpl.setInstance(paymentsManager);

    const section = document.createElement('settings-payments-section');
    section.prefs = {
      autofill: {
        credit_card_enabled: {value: true},
        payment_methods_mandatory_reauth: {value: true},
        payment_cvc_storage: {value: true},
      },
    };
    document.body.appendChild(section);
    await flushTasks();
    return section;
  }

  /**
   * Creates the Add Credit Card dialog. Simulate clicking "Add" button in
   * payments section.
   */
  async function createAddCreditCardDialog():
      Promise<SettingsCreditCardEditDialogElement> {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[]);
    // Simulate clicking "Add" button in payments section.
    assertFalse(!!section.shadowRoot!.querySelector(
        'settings-credit-card-edit-dialog'));
    const addCreditCardButton =
        section.shadowRoot!.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCreditCardButton);
    addCreditCardButton.click();
    flush();
    const creditCardDialog =
        section.shadowRoot!.querySelector('settings-credit-card-edit-dialog');
    assertTrue(!!creditCardDialog);
    return creditCardDialog;
  }

  /**
   * Creates the Add Credit Card dialog. Simulate clicking "Credit/Debit card"
   * option from dropdown list.
   */
  async function createAddCreditCardDialogFromDropdown():
      Promise<SettingsCreditCardEditDialogElement> {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[]);
    // Simulate clicking "Add" button in payments section.
    assertFalse(!!section.shadowRoot!.querySelector(
        'settings-credit-card-edit-dialog'));
    const dropdownAddPaymentMethodsButton =
        section.shadowRoot!.querySelector<CrButtonElement>(
            '#addPaymentMethods');
    assertTrue(!!dropdownAddPaymentMethodsButton);
    dropdownAddPaymentMethodsButton.click();
    flush();

    // Simulate clicking the 'Credit/Debit card' option in the menu.
    const addCardOption =
        section.shadowRoot!.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCardOption);
    addCardOption.click();
    flush();
    const creditCardDialog =
        section.shadowRoot!.querySelector('settings-credit-card-edit-dialog');
    assertTrue(!!creditCardDialog);
    return creditCardDialog;
  }

  /**
   * Creates the Add IBAN dialog. Simulate clicking "IBAN" option from the
   * dropdown list.
   */
  async function createAddIbanDialogFromDropdown():
      Promise<SettingsIbanEditDialogElement> {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[]);
    // Simulate clicking "Add" button in payments section.
    assertFalse(!!section.shadowRoot!.querySelector(
        'settings-credit-card-edit-dialog'));
    const addpaymentMethodsButton =
        section.shadowRoot!.querySelector<CrButtonElement>(
            '#addPaymentMethods');
    assertTrue(!!addpaymentMethodsButton);
    addpaymentMethodsButton.click();
    flush();

    // Simulate clicking the 'IBAN' option in the menu.
    const addIbanOption =
        section.shadowRoot!.querySelector<CrButtonElement>('#addIban');
    assertTrue(!!addIbanOption);
    addIbanOption.click();
    flush();
    const ibanDialog =
        section.shadowRoot!.querySelector('settings-iban-edit-dialog');
    assertTrue(!!ibanDialog);
    ibanDialog.$.saveButton.disabled = false;
    return ibanDialog!;
  }

  /**
   * Creates the Edit Credit Card dialog for existing local card by simulating
   * clicking three-dots menu button then clicking editing button of the first
   * card in the card list.
   */
  async function createEditCreditCardDialog(
      creditCards: chrome.autofillPrivate.CreditCardEntry[]):
      Promise<SettingsCreditCardEditDialogElement> {
    const section = await createPaymentsSection(creditCards, /*ibans=*/[]);
    // Simulate clicking three-dots menu button for the first card in the list.
    const rowShadowRoot =
        section.$.paymentsList.shadowRoot!
            .querySelector('settings-credit-card-list-entry')!.shadowRoot!;
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    // Simulate clicking the 'Edit' button in the menu.
    section.$.menuEditCreditCard.click();
    await flushTasks();
    const creditCardDialog =
        section.shadowRoot!.querySelector('settings-credit-card-edit-dialog');
    assertTrue(!!creditCardDialog);
    return creditCardDialog;
  }

  /**
   * Creates the Edit IBAN dialog for existing local IBANs by simulating
   * clicking the three-dots menu button then clicking the edit button of the
   * first IBAN in the list.
   */
  async function createEditIbanDialog(
      ibans: chrome.autofillPrivate.IbanEntry[]):
      Promise<SettingsIbanEditDialogElement> {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/ ibans);
    // Simulate clicking three-dots menu button for the first IBAN in the list.
    const firstEntry = section.$.paymentsList.shadowRoot!.querySelector(
        'settings-iban-list-entry');
    assertTrue(!!firstEntry);
    assertFalse(!!firstEntry.shadowRoot!.querySelector('#remoteIbanLink'));
    const menuButton =
        firstEntry.shadowRoot!.querySelector<HTMLElement>('#ibanMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Simulate clicking the 'Edit' button in the menu.
    const menuEditIban =
        section.shadowRoot!.querySelector<HTMLElement>('#menuEditIban');
    assertTrue(!!menuEditIban);
    menuEditIban.click();
    flush();
    const ibanDialog =
        section.shadowRoot!.querySelector('settings-iban-edit-dialog');
    assertTrue(!!ibanDialog);
    ibanDialog.$.saveButton.disabled = false;
    return ibanDialog;
  }

  function nextYear(): string {
    return (new Date().getFullYear() + 1).toString();
  }

  function farFutureYear(): string {
    return (new Date().getFullYear() + 15).toString();
  }

  function lastYear(): string {
    return (new Date().getFullYear() - 1).toString();
  }

  test('add card dialog', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: false,
    });
    const creditCardDialog = await createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');
    const nameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nameInput');
    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    const cvcInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>('#cvcInput');

    // Verify the nickname input field is shown when nickname management is
    // enabled.
    assertTrue(!!nicknameInput);
    assertTrue(!!nameInput);
    assertTrue(!!numberInput);
    assertTrue(!!cvcInput);
    // Verify the card number field is autofocused when nickname management is
    // enabled.
    assertTrue(numberInput.matches(':focus-within'));
  });

  test('add card dialog from dropdown list', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: true,
    });
    const creditCardDialog = await createAddCreditCardDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const nicknameInput = creditCardDialog.$.nicknameInput;
    const nameInput = creditCardDialog.$.nameInput;
    const numberInput = creditCardDialog.$.numberInput;
    const cvcInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>('#cvcInput');

    // Verify the nickname input field is shown when nickname management is
    // enabled.
    assertTrue(!!nicknameInput);
    assertTrue(!!nameInput);
    assertTrue(!!numberInput);
    assertTrue(!!cvcInput);
    // Verify the card number field is autofocused when nickname management is
    // enabled.
    assertTrue(numberInput.matches(':focus-within'));
  });

  test('save new card', async function() {
    const creditCardDialog = await createAddCreditCardDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Fill in name, card number, expiration year, card nickname and CVC, and
    // trigger the on-input handler.
    const nameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nameInput');
    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');
    const yearInput =
        creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>('#year');
    const cvcInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>('#cvcInput');
    assertTrue(!!cvcInput);
    nameInput!.value = 'Jane Doe';
    numberInput!.value = '4111111111111111';
    await typeInNickname(nicknameInput!, 'Grocery Card');
    yearInput!.value = nextYear();
    yearInput!.dispatchEvent(new CustomEvent('change'));
    cvcInput.value = '123';
    await cvcInput.updateComplete;
    flush();

    const expiredError =
        creditCardDialog.shadowRoot!.querySelector<HTMLElement>(
            '#expiredError');
    assertEquals('hidden', getComputedStyle(expiredError!).visibility);

    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#saveButton');
    assertFalse(saveButton!.disabled);

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    saveButton!.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-credit-card.
    // guid is undefined when saving a new card.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
    assertEquals(saveEvent.detail.expirationYear, nextYear());
    assertEquals('123', saveEvent.detail.cvc);
  });

  test('trim credit card when save', async function() {
    const creditCardDialog = await createAddCreditCardDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Set expiration year, fill in name, card number, and card nickname with
    // leading and trailing whitespaces, and trigger the on-input handler.
    const nameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nameInput');
    assertTrue(!!nameInput);
    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    assertTrue(!!numberInput);
    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');
    const yearInput =
        creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>('#year');
    const cvcInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>('#cvcInput');
    assertTrue(!!cvcInput);
    nameInput.value = '  Jane Doe  \n';
    numberInput.value = ' 4111111111111111 ';
    await Promise.all([
      nameInput.updateComplete,
      numberInput.updateComplete,
      typeInNickname(nicknameInput!, 'Grocery Card'),
    ]);
    yearInput!.value = nextYear();
    yearInput!.dispatchEvent(new CustomEvent('change'));
    cvcInput.value = ' ';
    await cvcInput.updateComplete;
    flush();

    const expiredError =
        creditCardDialog.shadowRoot!.querySelector<HTMLElement>(
            '#expiredError');
    assertEquals('hidden', getComputedStyle(expiredError!).visibility);

    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#saveButton');
    assertFalse(saveButton!.disabled);

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    saveButton!.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-credit-card.
    // guid is undefined when saving a new card.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
    assertEquals(saveEvent.detail.expirationYear, nextYear());
    // Due to PCI compliance we don't check the structure or length of the CVC,
    // thus don't make any updates to the same.
    assertEquals(' ', saveEvent.detail.cvc);
  });

  test('update local card value', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.name = 'Wrong name';
    creditCard.nickname = 'Shopping Card';
    // Set the expiration year to next year to avoid expired card.
    creditCard.expirationYear = nextYear();
    creditCard.cardNumber = '4444333322221111';
    creditCard.cvc = '123';
    const creditCardDialog = await createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // For editing local card, verify displaying with existing value.
    const nameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nameInput');
    assertTrue(!!nameInput);
    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');
    const numberInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#numberInput');
    assertTrue(!!numberInput);
    const yearInput =
        creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>('#year');
    const cvcInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>('#cvcInput');
    assertTrue(!!cvcInput);
    assertEquals(nameInput.value, 'Wrong name');
    assertEquals(nicknameInput!.value, 'Shopping Card');
    assertEquals(numberInput.value, '4444333322221111');
    assertEquals(yearInput!.value, nextYear());

    const expiredError =
        creditCardDialog.shadowRoot!.querySelector<HTMLElement>(
            '#expiredError');
    assertEquals('hidden', getComputedStyle(expiredError!).visibility);

    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#saveButton');
    assertFalse(saveButton!.disabled);

    // Update cardholder name, card number, expiration year and nickname, and
    // trigger the on-input handler.
    nameInput.value = 'Jane Doe';
    numberInput.value = '4111111111111111';
    await Promise.all([
      nameInput.updateComplete,
      numberInput.updateComplete,
      typeInNickname(nicknameInput!, 'Grocery Card'),
    ]);
    yearInput!.value = farFutureYear();
    yearInput!.dispatchEvent(new CustomEvent('change'));
    cvcInput.value = '098';
    await cvcInput.updateComplete;
    flush();

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    saveButton!.click();
    const saveEvent = await savedPromise;

    // Verify the updated values are correctly passed to save-credit-card.
    assertEquals(saveEvent.detail.guid, creditCard.guid);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
    assertEquals(saveEvent.detail.expirationYear, farFutureYear());
    assertEquals('098', saveEvent.detail.cvc);
  });

  test('show error message when input nickname is invalid', async function() {
    const creditCardDialog = await createAddCreditCardDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // User clicks on nickname input.
    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');
    assertTrue(!!nicknameInput);
    nicknameInput.focus();

    const validInputs = [
      '',
      ' ',
      '~@#$%^&**(){}|<>',
      'Grocery Card',
      'Two percent Cashback',
      /* UTF-16 hex encoded credit card emoji */ 'Chase Freedom \uD83D\uDCB3',
    ];
    for (const nickname of validInputs) {
      await typeInNickname(nicknameInput, nickname);
      assertFalse(nicknameInput.invalid);
      // Error message is hidden for valid nickname input.
      assertEquals(
          'hidden', getComputedStyle(nicknameInput.$.error).visibility);
    }

    // Verify invalid nickname inputs.
    const invalidInputs = [
      '12345',
      '2abc',
      'abc3',
      'abc4de',
      'a 1 b',
      /* UTF-16 hex encoded digt 7 emoji */ 'Digit emoji: \u0037\uFE0F\u20E3',
    ];
    for (const nickname of invalidInputs) {
      await typeInNickname(nicknameInput, nickname);
      assertTrue(nicknameInput.invalid);
      assertNotEquals('', nicknameInput.errorMessage);
      // Error message is shown for invalid nickname input.
      assertEquals(
          'visible', getComputedStyle(nicknameInput.$.error).visibility);
    }
    // The error message is still shown even when user does not focus on the
    // nickname field.
    nicknameInput.blur();
    await nicknameInput.updateComplete;
    assertTrue(nicknameInput.invalid);
    assertEquals('visible', getComputedStyle(nicknameInput.$.error).visibility);
  });

  test('disable save button when input nickname is invalid', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.name = 'Wrong name';
    // Set the expiration year to next year to avoid expired card.
    creditCard.expirationYear = nextYear();
    creditCard.cardNumber = '4444333322221111';
    // Edit dialog for an existing card with no nickname.
    const creditCardDialog = await createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');
    // Save button is enabled for existing card with no nickname.
    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#saveButton');
    assertFalse(saveButton!.disabled);
    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');

    await typeInNickname(nicknameInput!, 'invalid: 123');
    // Save button is disabled since the nickname is invalid.
    assertTrue(saveButton!.disabled);

    await typeInNickname(nicknameInput!, 'valid nickname');
    // Save button is back to enabled since user updates with a valid nickname.
    assertFalse(saveButton!.disabled);
  });

  test('only show nickname character count when focused', async function() {
    const creditCardDialog = await createAddCreditCardDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    const nicknameInput =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#nicknameInput');
    assertTrue(!!nicknameInput);
    const characterCount =
        creditCardDialog.shadowRoot!.querySelector<HTMLElement>('#charCount')!;
    // Character count is not shown when add card dialog is open (not focusing
    // on nickname input field).
    assertFalse(isVisible(characterCount));

    // User clicks on nickname input.
    nicknameInput!.focus();
    // Character count is shown when nickname input field is focused.
    assertTrue(isVisible(characterCount));
    // For new card, the nickname is unset.
    assertTrue(characterCount.textContent!.includes('0/25'));

    // User types in one character. Ensure the character count is dynamically
    // updated.
    await typeInNickname(nicknameInput!, 'a');
    assertTrue(characterCount.textContent!.includes('1/25'));
    // User types in total 5 characters.
    await typeInNickname(nicknameInput!, 'abcde');
    assertTrue(characterCount.textContent!.includes('5/25'));

    // User click outside of nickname input, the character count isn't shown.
    nicknameInput!.blur();
    await nicknameInput.updateComplete;
    assertFalse(isVisible(characterCount));

    // User clicks on nickname input again.
    nicknameInput!.focus();
    await nicknameInput.updateComplete;
    // Character count is shown when nickname input field is re-focused.
    assertTrue(isVisible(characterCount));
    assertTrue(characterCount.textContent!.includes('5/25'));
  });

  test('expired card', async function() {
    const creditCard = createCreditCardEntry();
    // Set the expiration year to the previous year to simulate expired card.
    creditCard.expirationYear = lastYear();
    // Edit dialog for an existing card with no nickname.
    const creditCardDialog = await createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Verify save button is disabled for expired credit card.
    const saveButton =
        creditCardDialog.shadowRoot!.querySelector<CrInputElement>(
            '#saveButton');
    const expiredError =
        creditCardDialog.shadowRoot!.querySelector<HTMLElement>(
            '#expiredError');
    // The expired error message is shown.
    assertEquals('visible', getComputedStyle(expiredError!).visibility);
    // Check a11y attributes added for correct error announcement.
    assertEquals('alert', expiredError!.getAttribute('role'));

    const monthInput =
        creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>('#month');
    const yearInput =
        creditCardDialog.shadowRoot!.querySelector<HTMLSelectElement>('#year');
    for (const select of [monthInput!, yearInput!]) {
      assertEquals('true', select.getAttribute('aria-invalid'));
      assertEquals(expiredError!.id, select.getAttribute('aria-errormessage'));
    }

    // Update the expiration year to next year to avoid expired card.
    yearInput!.value = nextYear();
    yearInput!.dispatchEvent(new CustomEvent('change'));
    flush();

    // Expired error message is hidden for valid expiration date.
    assertEquals('hidden', getComputedStyle(expiredError!).visibility);
    assertFalse(saveButton!.disabled);
    // Check a11y attributes for expiration error removed.
    assertEquals(null, expiredError!.getAttribute('role'));
    for (const select of [monthInput!, yearInput!]) {
      assertEquals('false', select.getAttribute('aria-invalid'));
      assertEquals(null, select.getAttribute('aria-errormessage'));
    }
  });

  test('add iban dialog from dropdown list', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: true,
    });
    const ibanDialog = await createAddIbanDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    const nicknameInput = ibanDialog.$.nicknameInput;
    const valueInput = ibanDialog.$.valueInput;

    // Verify the value and nickname input fields are shown.
    assertTrue(!!valueInput);
    assertTrue(!!nicknameInput);
  });

  test('save new IBAN', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: true,
    });
    const ibanDialog = await createAddIbanDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    const nicknameInput = ibanDialog.$.nicknameInput;
    const valueInput = ibanDialog.$.valueInput;
    const characterCount =
        ibanDialog.shadowRoot!.querySelector<HTMLElement>('#charCount');

    assertTrue(!!characterCount);
    assertFalse(isVisible(characterCount));
    // User clicks on nickname input.
    nicknameInput!.focus();
    // Character count is shown when nickname input field is focused.
    assertTrue(isVisible(characterCount));
    // For new IBAN, the nickname is unset.
    assertTrue(characterCount.textContent!.includes('0/25'));

    // Fill in IBAN value and nickname, and trigger the on-input handler.
    nicknameInput.value = 'My doctor\'s IBAN';
    await nicknameInput.updateComplete;
    assertTrue(characterCount.textContent!.includes('16/25'));

    valueInput.value = 'IT60X0542811101000000123456';

    // IBAN validation is asynchronous, so wait for it to complete and the save
    // button state to be updated.
    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    await ibanValidated(paymentsManager);

    const savedPromise = eventToPromise('save-iban', ibanDialog);
    const saveButton = ibanDialog.$.saveButton;
    saveButton!.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-iban.
    // `guid` is undefined when saving a new IBAN.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.value, 'IT60X0542811101000000123456');
    assertEquals(saveEvent.detail.nickname, 'My doctor\'s IBAN');
  });

  test('trim IBAN when saving', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: true,
    });
    const ibanDialog = await createAddIbanDialogFromDropdown();

    // Wait for the dialog to open.
    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    // Fill in IBAN value and nickname, and trigger the on-input handler.
    const nicknameInput = ibanDialog.$.nicknameInput;
    const valueInput = ibanDialog.$.valueInput;
    nicknameInput.value = '   My doctor\'s IBAN  ';
    valueInput.value = '  IT60 X054 2811 1010 0000 0123 456 ';

    // IBAN validation is asynchronous, so wait for it to complete and the save
    // button state to be updated.
    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    await ibanValidated(paymentsManager);

    const savedPromise = eventToPromise('save-iban', ibanDialog);
    const saveButton = ibanDialog.$.saveButton;
    saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-iban.
    // `guid` is undefined when saving a new IBAN.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.value, 'IT60X0542811101000000123456');
    assertEquals(saveEvent.detail.nickname, 'My doctor\'s IBAN');
  });

  test('update local IBAN value', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: true,
    });
    const iban =
        createIbanEntry('IE64 IRCE 9205 0112 3456 78', 'My teacher\'s IBAN');
    const ibanDialog = await createEditIbanDialog([iban]);

    // Wait for the dialog to open.
    await whenAttributeIs(ibanDialog.$.dialog, 'open', '');

    // Update IBAN value and nickname, and trigger the on-input handler.
    const nicknameInput = ibanDialog.$.nicknameInput;
    const valueInput = ibanDialog.$.valueInput;
    valueInput.value = 'DE75 5121 0800 1245 1261 99';
    nicknameInput.value = 'My brother\'s IBAN';

    // IBAN validation is asynchronous, so wait for it to complete and the save
    // button state to be updated.
    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    await ibanValidated(paymentsManager);

    const savedPromise = eventToPromise('save-iban', ibanDialog);
    const saveButton = ibanDialog.$.saveButton;
    saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the updated values are correctly passed to save-iban.
    assertEquals(saveEvent.detail.guid, iban.guid);
    assertEquals(saveEvent.detail.value, 'DE75512108001245126199');
    assertEquals(saveEvent.detail.nickname, 'My brother\'s IBAN');
  });

});
