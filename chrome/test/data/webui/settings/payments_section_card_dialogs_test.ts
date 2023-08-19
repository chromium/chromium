// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsSimpleConfirmationDialogElement, PaymentsManagerImpl, SettingsCreditCardEditDialogElement, SettingsVirtualCardUnenrollDialogElement} from 'chrome://settings/lazy_load.js';
import {CrButtonElement, loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import {createCreditCardEntry, createEmptyCreditCardEntry, TestPaymentsManager} from './autofill_fake_data.js';
import {createPaymentsSection, getDefaultExpectations, getLocalAndServerCreditCardListItems, getCardRowShadowRoot} from './payments_section_utils.js';

// clang-format on

suite('PaymentsSectionCardDialogs', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      virtualCardEnrollmentEnabled: true,
      showIbansSettings: true,
    });
  });

  /**
   * Creates the Edit Credit Card dialog.
   */
  function createCreditCardDialog(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry):
      SettingsCreditCardEditDialogElement {
    const dialog = document.createElement('settings-credit-card-edit-dialog');
    dialog.creditCard = creditCardItem;
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

  test('verify save new credit card', function() {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          // Not expired, but still can't be saved, because there's no
          // name.
          const expiredError =
              creditCardDialog.shadowRoot!.querySelector<HTMLElement>(
                  '#expiredError');
          assertEquals('hidden', getComputedStyle(expiredError!).visibility);

          const saveButton =
              creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
                  '#saveButton');
          assertTrue(saveButton!.disabled);

          // Add a name.
          creditCardDialog.set('name_', 'Jane Doe');
          flush();

          assertEquals('hidden', getComputedStyle(expiredError!).visibility);
          assertFalse(saveButton!.disabled);

          const savedPromise =
              eventToPromise('save-credit-card', creditCardDialog);
          saveButton!.click();
          return savedPromise;
        })
        .then(function(event) {
          assertEquals(creditCard.guid, event.detail.guid);
        });
  });

  test('verifyNotEditedEntryAfterCancel', async function() {
    const creditCard = createCreditCardEntry();
    let creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Edit a entry.
    creditCardDialog.set('name_', 'EditedName');
    creditCardDialog.set('nickname_', 'NickName');
    creditCardDialog.set('cardNumber_', '0000000000001234');
    flush();

    const cancelButton =
        creditCardDialog.shadowRoot!.querySelector<CrButtonElement>(
            '#cancelButton');
    cancelButton!.click();

    await eventToPromise('close', creditCardDialog);

    creditCardDialog = createCreditCardDialog(creditCard);
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    assertEquals(creditCardDialog.get('name_'), creditCard.name);
    assertEquals(creditCardDialog.get('cardNumber_'), creditCard.cardNumber);
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
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
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
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
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
});
