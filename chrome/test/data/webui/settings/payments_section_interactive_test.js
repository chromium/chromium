// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {createCreditCardEntry, createEmptyCreditCardEntry, TestPaymentsManager} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {eventToPromise, isVisible, whenAttributeIs} from 'chrome://test/test_util.m.js';
// clang-format on

/**
 * Helper function to simulate typing in nickname in the nickname field.
 * @param {!Element} nicknameInput
 * @param {string} nickname
 */
function typeInNickname(nicknameInput, nickname) {
  nicknameInput.value = nickname;
  nicknameInput.fire('input');
}

suite('PaymentsSectionCreditCardEditDialogTest', function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  /**
   * Creates the payments section for the given credit card list.
   * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
   * @return {!Object}
   */
  function createPaymentsSection(creditCards) {
    // Override the PaymentsManagerImpl for testing.
    const paymentsManager = new TestPaymentsManager();
    paymentsManager.data.creditCards = creditCards;
    PaymentsManagerImpl.instance_ = paymentsManager;

    const section = document.createElement('settings-payments-section');
    document.body.appendChild(section);
    flush();
    return section;
  }

  /**
   * Creates the Add Credit Card dialog. Simulate clicking "Add" button in
   * payments section.
   * @return {!Object}
   */
  function createAddCreditCardDialog() {
    const section = createPaymentsSection(/*creditCards=*/[]);
    // Simulate clicking "Add" button in payments section.
    assertTrue(!!section.$$('#addCreditCard'));
    assertFalse(!!section.$$('settings-credit-card-edit-dialog'));
    section.$$('#addCreditCard').click();
    flush();
    assertTrue(!!section.$$('settings-credit-card-edit-dialog'));
    const creditCardDialog = section.$$('settings-credit-card-edit-dialog');
    return creditCardDialog;
  }

  /**
   * Creates the Edit Credit Card dialog for existing local card by simulating
   * clicking three-dots menu button then clicking editing button of the first
   * card in the card list.
   * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
   * @return {!Object}
   */
  function createEditCreditCardDialog(creditCards) {
    const section = createPaymentsSection(creditCards);
    // Simulate clicking three-dots menu button for the first card in the list.
    const rowShadowRoot = section.$$('#paymentsList')
                              .$$('settings-credit-card-list-entry')
                              .shadowRoot;
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    // Simulate clicking the 'Edit' button in the menu.
    const menu = section.$.creditCardSharedMenu;
    const editButton = menu.querySelector('#menuEditCreditCard');
    editButton.click();
    flush();
    assertTrue(!!section.$$('settings-credit-card-edit-dialog'));
    const creditCardDialog = section.$$('settings-credit-card-edit-dialog');
    return creditCardDialog;
  }

  /** @return {string} */
  function nextYear() {
    return (new Date().getFullYear() + 1).toString();
  }

  /** @return {string} */
  function farFutureYear() {
    return (new Date().getFullYear() + 15).toString();
  }

  /** @return {string} */
  function lastYear() {
    return (new Date().getFullYear() - 1).toString();
  }

  test('add card dialog', async function() {
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Verify the nickname input field is shown when nickname management is
    // enabled.
    assertTrue(!!creditCardDialog.$$('#nicknameInput'));
    assertTrue(!!creditCardDialog.$$('#nameInput'));

    // Verify the card number field is autofocused when nickname management is
    // enabled.
    assertTrue(creditCardDialog.$$('#numberInput').matches(':focus-within'));
  });

  test('save new card', async function() {
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Fill in name, card number, expiration year and card nickname, and trigger
    // the on-input handler.
    creditCardDialog.$$('#nameInput').value = 'Jane Doe';
    creditCardDialog.$$('#numberInput').value = '4111111111111111';
    typeInNickname(creditCardDialog.$$('#nicknameInput'), 'Grocery Card');
    creditCardDialog.$.year.value = nextYear();
    creditCardDialog.$.year.dispatchEvent(new CustomEvent('change'));
    flush();

    const expiredError = creditCardDialog.$$('#expired-error');
    assertEquals('hidden', getComputedStyle(expiredError).visibility);
    assertFalse(creditCardDialog.$.saveButton.disabled);

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    creditCardDialog.$.saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-credit-card.
    // guid is undefined when saving a new card.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
    assertEquals(saveEvent.detail.expirationYear, nextYear());
  });

  test('trim credit card when save', async function() {
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Set expiration year, fill in name, card number, and card nickname with
    // leading and trailing whitespaces, and trigger the on-input handler.
    creditCardDialog.$$('#nameInput').value = '  Jane Doe  \n';
    creditCardDialog.$$('#numberInput').value = ' 4111111111111111 ';
    typeInNickname(creditCardDialog.$$('#nicknameInput'), ' Grocery Card  ');
    creditCardDialog.$.year.value = nextYear();
    creditCardDialog.$.year.dispatchEvent(new CustomEvent('change'));
    flush();

    const expiredError = creditCardDialog.$$('#expired-error');
    assertEquals('hidden', getComputedStyle(expiredError).visibility);
    assertFalse(creditCardDialog.$.saveButton.disabled);

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    creditCardDialog.$.saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-credit-card.
    // guid is undefined when saving a new card.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
    assertEquals(saveEvent.detail.expirationYear, nextYear());
  });

  test('update local card value', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.name = 'Wrong name';
    creditCard.nickname = 'Shopping Card';
    // Set the expiration year to next year to avoid expired card.
    creditCard.expirationYear = nextYear();
    creditCard.cardNumber = '4444333322221111';
    const creditCardDialog = createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // For editing local card, verify displaying with existing value.
    assertEquals(creditCardDialog.$$('#nameInput').value, 'Wrong name');
    assertEquals(creditCardDialog.$$('#nicknameInput').value, 'Shopping Card');
    assertEquals(creditCardDialog.$$('#numberInput').value, '4444333322221111');
    assertEquals(creditCardDialog.$.year.value, nextYear());

    const expiredError = creditCardDialog.$$('#expired-error');
    assertEquals('hidden', getComputedStyle(expiredError).visibility);
    assertFalse(creditCardDialog.$.saveButton.disabled);

    // Update cardholder name, card number, expiration year and nickname, and
    // trigger the on-input handler.
    creditCardDialog.$$('#nameInput').value = 'Jane Doe';
    creditCardDialog.$$('#numberInput').value = '4111111111111111';
    typeInNickname(creditCardDialog.$$('#nicknameInput'), 'Grocery Card');
    creditCardDialog.$.year.value = farFutureYear();
    creditCardDialog.$.year.dispatchEvent(new CustomEvent('change'));
    flush();

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    creditCardDialog.$.saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the updated values are correctly passed to save-credit-card.
    assertEquals(saveEvent.detail.guid, creditCard.guid);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
    assertEquals(saveEvent.detail.expirationYear, farFutureYear());
  });

  test('show error message when input nickname is invalid', async function() {
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // User clicks on nickname input.
    const nicknameInput = creditCardDialog.$$('#nicknameInput');
    assertTrue(!!nicknameInput);
    nicknameInput.focus();

    const validInputs = [
      '', ' ', '~@#$%^&**(){}|<>', 'Grocery Card', 'Two percent Cashback',
      /* UTF-16 hex encoded credit card emoji */ 'Chase Freedom \uD83D\uDCB3'
    ];
    for (const nickname of validInputs) {
      typeInNickname(nicknameInput, nickname);
      assertFalse(nicknameInput.invalid);
      // Error message is hidden for valid nickname input.
      assertEquals(
          'hidden', getComputedStyle(nicknameInput.$.error).visibility);
    }

    // Verify invalid nickname inputs.
    const invalidInputs = [
      '12345', '2abc', 'abc3', 'abc4de', 'a 1 b',
      /* UTF-16 hex encoded digt 7 emoji */ 'Digit emoji: \u0037\uFE0F\u20E3'
    ];
    for (const nickname of invalidInputs) {
      typeInNickname(nicknameInput, nickname);
      assertTrue(nicknameInput.invalid);
      assertNotEquals('', nicknameInput.errorMessage);
      // Error message is shown for invalid nickname input.
      assertEquals(
          'visible', getComputedStyle(nicknameInput.$.error).visibility);
    }
    // The error message is still shown even when user does not focus on the
    // nickname field.
    nicknameInput.blur();
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
    const creditCardDialog = createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');
    // Save button is enabled for existing card with no nickname.
    assertFalse(creditCardDialog.$.saveButton.disabled);
    const nicknameInput = creditCardDialog.$$('#nicknameInput');

    typeInNickname(nicknameInput, 'invalid: 123');
    // Save button is disabled since the nickname is invalid.
    assertTrue(creditCardDialog.$.saveButton.disabled);

    typeInNickname(nicknameInput, 'valid nickname');
    // Save button is back to enabled since user updates with a valid nickname.
    assertFalse(creditCardDialog.$.saveButton.disabled);
  });

  test('only show nickname character count when focused', async function() {
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    const nicknameInput = creditCardDialog.$$('#nicknameInput');
    assertTrue(!!nicknameInput);
    const characterCount = creditCardDialog.$$('#charCount');
    // Character count is not shown when add card dialog is open (not focusing
    // on nickname input field).
    assertFalse(isVisible(characterCount));

    // User clicks on nickname input.
    nicknameInput.focus();
    // Character count is shown when nickname input field is focused.
    assertTrue(isVisible(characterCount));
    // For new card, the nickname is unset.
    assertTrue(characterCount.textContent.includes('0/25'));

    // User types in one character. Ensure the character count is dynamically
    // updated.
    typeInNickname(nicknameInput, 'a');
    assertTrue(characterCount.textContent.includes('1/25'));
    // User types in total 5 characters.
    typeInNickname(nicknameInput, 'abcde');
    assertTrue(characterCount.textContent.includes('5/25'));

    // User click outside of nickname input, the character count isn't shown.
    nicknameInput.blur();
    assertFalse(isVisible(characterCount));

    // User clicks on nickname input again.
    nicknameInput.focus();
    // Character count is shown when nickname input field is re-focused.
    assertTrue(isVisible(characterCount));
    assertTrue(characterCount.textContent.includes('5/25'));
  });

  test('expired card', async function() {
    const creditCard = createCreditCardEntry();
    // Set the expiration year to the previous year to simulate expired card.
    creditCard.expirationYear = lastYear();
    // Edit dialog for an existing card with no nickname.
    const creditCardDialog = createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Verify save button is disabled for expired credit card.
    assertTrue(creditCardDialog.$.saveButton.disabled);
    const expiredError = creditCardDialog.$$('#expired-error');
    // The expired error message is shown.
    assertEquals('visible', getComputedStyle(expiredError).visibility);
    // Check a11y attributes added for correct error announcement.
    assertEquals('alert', expiredError.getAttribute('role'));
    for (const select of [creditCardDialog.$.month, creditCardDialog.$.year]) {
      assertEquals('true', select.getAttribute('aria-invalid'));
      assertEquals('expired-error', select.getAttribute('aria-errormessage'));
    }

    // Update the expiration year to next year to avoid expired card.
    creditCardDialog.$.year.value = nextYear();
    creditCardDialog.$.year.dispatchEvent(new CustomEvent('change'));
    flush();

    // Expired error message is hidden for valid expiration date.
    assertEquals('hidden', getComputedStyle(expiredError).visibility);
    assertFalse(creditCardDialog.$.saveButton.disabled);
    // Check a11y attributes for expiration error removed.
    assertEquals(null, expiredError.getAttribute('role'));
    for (const select of [creditCardDialog.$.month, creditCardDialog.$.year]) {
      assertEquals('false', select.getAttribute('aria-invalid'));
      assertEquals(null, select.getAttribute('aria-errormessage'));
    }
  });
});
