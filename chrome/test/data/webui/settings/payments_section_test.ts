// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl, SettingsCreditCardEditDialogElement, SettingsIbanEditDialogElement, SettingsPaymentsSectionElement, SettingsVirtualCardUnenrollDialogElement} from 'chrome://settings/lazy_load.js';
import {CrButtonElement, loadTimeData, MetricsBrowserProxyImpl, PrivacyElementInteractions, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createCreditCardEntry, createIbanEntry, createEmptyCreditCardEntry, PaymentsManagerExpectations,TestPaymentsManager} from './passwords_and_autofill_fake_data.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('PaymentSectionUiTest', function() {
  test('testAutofillExtensionIndicator', function() {
    // Initializing with fake prefs
    const section = document.createElement('settings-payments-section');
    section.prefs = {
      autofill: {credit_card_enabled: {}, credit_card_fido_auth_enabled: {}},
    };
    document.body.appendChild(section);

    assertFalse(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
    section.set('prefs.autofill.credit_card_enabled.extensionId', 'test-id-1');
    section.set(
        'prefs.autofill.credit_card_fido_auth_enabled.extensionId',
        'test-id-2');
    flush();

    assertTrue(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
  });
});

suite('PaymentsSection', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      removeCardExpirationAndTypeTitles: true,
      virtualCardEnrollmentEnabled: true,
      showIbansSettings: true,
    });
  });

  /**
   * Creates the payments autofill section for the given list.
   * @param {!Object} prefValues
   */
  async function createPaymentsSection(
      creditCards: chrome.autofillPrivate.CreditCardEntry[],
      ibans: chrome.autofillPrivate.IbanEntry[], upiIds: string[],
      prefValues: any): Promise<SettingsPaymentsSectionElement> {
    // Override the PaymentsManagerImpl for testing.
    const paymentsManager = new TestPaymentsManager();
    paymentsManager.data.creditCards = creditCards;
    paymentsManager.data.ibans = ibans;
    paymentsManager.data.upiIds = upiIds;
    PaymentsManagerImpl.setInstance(paymentsManager);

    const section = document.createElement('settings-payments-section');
    section.prefs = {autofill: prefValues};
    document.body.appendChild(section);
    await flushTasks();

    return section;
  }

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
   * Creates the Edit IBAN dialog.
   */
  function createIbanDialog(ibanItem: chrome.autofillPrivate.IbanEntry):
      SettingsIbanEditDialogElement {
    const dialog = document.createElement('settings-iban-edit-dialog');
    dialog.iban = ibanItem;
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

  // Fakes the existence of a platform authenticator.
  function addFakePlatformAuthenticator() {
    (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
        .setIsUserVerifyingPlatformAuthenticatorAvailable(true);
  }


  /**
   * Returns an array containing the local and server credit card items.
   */
  function getLocalAndServerCreditCardListItems() {
    return document.body.querySelector('settings-payments-section')!.shadowRoot!
        .querySelector('#paymentsList')!.shadowRoot!.querySelectorAll(
            'settings-credit-card-list-entry')!;
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
   * Returns the shadow root of the card row from the specified list of
   * payment methods.
   */
  function getCardRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
    const row = paymentsList.shadowRoot!.querySelector(
        'settings-credit-card-list-entry');
    assertTrue(!!row);
    return row.shadowRoot!;
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

  /**
   * Returns the shadow root of the UPI ID row from the specified list of
   * payment methods.
   */
  function getUPIRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
    const row =
        paymentsList.shadowRoot!.querySelector('settings-upi-id-list-entry');
    assertTrue(!!row);
    return row.shadowRoot!;
  }

  /**
   * Returns the default expectations from TestPaymentsManager. Adjust the
   * values as needed.
   */
  function getDefaultExpectations(): PaymentsManagerExpectations {
    const expected = new PaymentsManagerExpectations();
    expected.requestedCreditCards = 1;
    expected.listeningCreditCards = 1;
    expected.removedCreditCards = 0;
    expected.clearedCachedCreditCards = 0;
    expected.addedVirtualCards = 0;
    expected.requestedIbans = 1;
    expected.removedIbans = 0;
    return expected;
  }

  test('verifyNoCreditCards', async function() {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(0, getLocalAndServerCreditCardListItems().length);

    const noPaymentMethodsLabel =
        creditCardList.shadowRoot!.querySelector<HTMLElement>(
            '#noPaymentMethodsLabel');
    assertTrue(!!noPaymentMethodsLabel);
    assertFalse(noPaymentMethodsLabel.hidden);

    const creditCardsHeading =
        creditCardList.shadowRoot!.querySelector<HTMLElement>(
            '#creditCardsHeading');
    assertTrue(!!creditCardsHeading);
    assertTrue(creditCardsHeading.hidden);

    assertFalse(section.$.autofillCreditCardToggle.disabled);

    const addPaymentMethodsButton =
        section.shadowRoot!.querySelector<CrButtonElement>(
            '#addPaymentMethods');
    assertTrue(!!addPaymentMethodsButton);
    assertFalse(addPaymentMethodsButton.disabled);
  });

  test('verifyCreditCardsDisabled', async function() {
    loadTimeData.overrideValues({
      showIbansSettings: false,
    });
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: false}});

    assertFalse(section.$.autofillCreditCardToggle.disabled);
    const addCreditCardButton =
        section.shadowRoot!.querySelector<CrButtonElement>('#addCreditCard');
    assertTrue(!!addCreditCardButton);
    assertTrue(addCreditCardButton.hidden);
  });

  test('verifyCreditCardCount', async function() {
    loadTimeData.overrideValues({
      removeCardExpirationAndTypeTitles: true,
    });
    const creditCards = [
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
    ];

    const section = await createPaymentsSection(
        creditCards, /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(
        creditCards.length, getLocalAndServerCreditCardListItems().length);

    const noPaymentMethodsLabel =
        creditCardList.shadowRoot!.querySelector<HTMLElement>(
            '#noPaymentMethodsLabel');
    assertTrue(!!noPaymentMethodsLabel);
    assertTrue(noPaymentMethodsLabel.hidden);

    const creditCardsHeading =
        creditCardList.shadowRoot!.querySelector<HTMLElement>(
            '#creditCardsHeading');
    assertTrue(!!creditCardsHeading);
    assertTrue(creditCardsHeading.hidden);

    assertFalse(section.$.autofillCreditCardToggle.disabled);

    const addPaymentMethodsButton =
        section.shadowRoot!.querySelector<CrButtonElement>(
            '#addPaymentMethods');
    assertTrue(!!addPaymentMethodsButton);
    assertFalse(addPaymentMethodsButton.disabled);
  });

  test('verifyCreditCardFields', async function() {
    const creditCard = createCreditCardEntry();
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertEquals(
        creditCard.metadata!.summaryLabel,
        rowShadowRoot.querySelector<HTMLElement>(
                         '#summaryLabel')!.textContent!.trim());
    assertEquals(
        creditCard.expirationMonth + '/' + creditCard.expirationYear,
        rowShadowRoot.querySelector<HTMLElement>(
                         '#creditCardExpiration')!.textContent!.trim());
  });

  test('verifyCreditCardRowButtonIsDropdownWhenLocal', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertFalse(!!outlinkButton);
  });

  test('verifyCreditCardMoreDetailsTitle', async function() {
    let creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const updateCreditCardCallback =
        (creditCard: chrome.autofillPrivate.CreditCardEntry) => {
          (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
              .lastCallback.setPersonalDataManagerListener!
              ([], [creditCard], [], {
                email: 'stub-user@example.com',
                isSyncEnabledForAutofillProfiles: true,
              });
          flush();
        };

    // Case 1: a card with a nickname
    creditCard = createCreditCardEntry();
    creditCard.nickname = 'My card name';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for My card name', menuButton!.getAttribute('title'));

    // Case 2: a card without nickname
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = 'Visa';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Visa ending in 1234',
        menuButton!.getAttribute('title'));

    // Case 3: a card without network
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = undefined;
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Card ending in 1234',
        menuButton!.getAttribute('title'));

    // Case 4: a card without number
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = undefined;
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Jane Doe', menuButton!.getAttribute('title'));
  });

  test('verifyCreditCardRowButtonIsOutlinkWhenRemote', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertFalse(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
  });

  test('verifyPaymentSummarySublabelIsHidden', async function() {
    loadTimeData.overrideValues({
      removeCardExpirationAndTypeTitles: false,
    });
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                   .querySelector<HTMLElement>('#summarySublabel')!.hidden);
  });

  test('verifyCreditCardSummarySublabelWithExpirationDate', async function() {
    loadTimeData.overrideValues({
      removeCardExpirationAndTypeTitles: true,
    });
    const creditCard = createCreditCardEntry();

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
    assertEquals(
        creditCard.expirationMonth + '/' + creditCard.expirationYear +
            '\u00a0|\u00a0' + creditCard.metadata!.summarySublabel,
        getCardRowShadowRoot(section.$.paymentsList)
            .querySelector<HTMLElement>(
                '#summarySublabel')!.textContent!.trim());
  });

  test(
      'verifyCreditCardRowButtonIsDropdownWhenVirtualCardEnrollEligible',
      async function() {
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
        const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
        const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
        assertTrue(!!menuButton);
        const outlinkButton =
            rowShadowRoot.querySelector('cr-icon-button.icon-external');
        assertFalse(!!outlinkButton);
      });

  test('verifyCreditCardSummarySublabelWhenSublabelIsValid', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
  });

  test(
      'verifyCreditCardSummarySublabelWhenSublabelIsInvalid', async function() {
        loadTimeData.overrideValues({
          removeCardExpirationAndTypeTitles: false,
        });
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        creditCard.metadata!.summarySublabel = '';
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

        const creditCardList = section.$.paymentsList;
        assertTrue(!!creditCardList);
        assertEquals(1, getLocalAndServerCreditCardListItems().length);
        assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                       .querySelector<HTMLElement>('#summarySublabel')!.hidden);
      });

  test(
      'verifyCreditCardSummarySublabelWhenVirtualCardAvailable',
      async function() {
        loadTimeData.overrideValues({
          removeCardExpirationAndTypeTitles: true,
        });
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

        const creditCardList = section.$.paymentsList;
        assertTrue(!!creditCardList);
        assertEquals(1, getLocalAndServerCreditCardListItems().length);
        assertFalse(
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>('#summarySublabel')!.hidden);
        assertEquals(
            creditCard.expirationMonth + '/' + creditCard.expirationYear +
                '\u00a0|\u00a0' +
                'Virtual card available',
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>(
                    '#summarySublabel')!.textContent!.trim());
      });

  test(
      'verifyCreditCardSummarySublabelWhenVirtualCardTurnedOn',
      async function() {
        loadTimeData.overrideValues({
          removeCardExpirationAndTypeTitles: true,
        });
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
        creditCard.metadata!.isVirtualCardEnrolled = true;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

        const creditCardList = section.$.paymentsList;
        assertTrue(!!creditCardList);
        assertEquals(1, getLocalAndServerCreditCardListItems().length);
        assertFalse(
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>('#summarySublabel')!.hidden);
        assertEquals(
            creditCard.expirationMonth + '/' + creditCard.expirationYear +
                '\u00a0|\u00a0' +
                'Virtual card turned on',
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>(
                    '#summarySublabel')!.textContent!.trim());
      });

  test(
      'verifyCreditCardSummarySublabelExpirationAndVirtualCardAvailable',
      async function() {
        loadTimeData.overrideValues({
          removeCardExpirationAndTypeTitles: true,
        });
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
        creditCard.metadata!.isVirtualCardEnrolled = true;
        const section = await createPaymentsSection(
            [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

        const creditCardList = section.$.paymentsList;
        assertTrue(!!creditCardList);
        assertEquals(1, getLocalAndServerCreditCardListItems().length);
        assertFalse(
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>('#summarySublabel')!.hidden);
        assertEquals(
            creditCard.expirationMonth + '/' + creditCard.expirationYear +
                '\u00a0|\u00a0' +
                'Virtual card turned on',
            getCardRowShadowRoot(section.$.paymentsList)
                .querySelector<HTMLElement>(
                    '#summarySublabel')!.textContent!.trim());
      });

  test('verifyPaymentsLabel', async function() {
    loadTimeData.overrideValues({
      removeCardExpirationAndTypeTitles: false,
    });
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#paymentsLabel')!.hidden);
    assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                   .querySelector<HTMLElement>('#paymentsIndicator')!.hidden);
  });

  test('verifyPaymentsIndicator', async function() {
    loadTimeData.overrideValues({
      removeCardExpirationAndTypeTitles: true,
    });
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                   .querySelector<HTMLElement>('#paymentsLabel')!.hidden);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#paymentsIndicator')!.hidden);
  });

  test('verifyCardImage', async function() {
    loadTimeData.overrideValues({
      removeCardExpirationAndTypeTitles: true,
    });
    const creditCard = createCreditCardEntry();
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#cardImage')!.hidden);
  });

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

  test('verifyLocalCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    // When credit card is local, |isCached| will be undefined.
    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isCached = undefined;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Local credit cards will show the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertFalse(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyCachedCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Cached remote CCs will show overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertFalse(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyNotCachedCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // No overflow menu when not cached.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
  });

  test('verifyVirtualCardEligibleCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Server cards that are eligible for virtual card enrollment should show
    // the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertFalse(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyVirtualCardEnrolledCreditCardMenu', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = true;

    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Server cards that are eligible for virtual card enrollment should show
    // the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertFalse(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyClearCachedCreditCardClicked', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = true;
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

    assertFalse(section.$.menuClearCreditCard.hidden);
    section.$.menuClearCreditCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.clearedCachedCreditCards = 1;
    paymentsManager.assertExpectations(expectations);
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

    const confirmationDialog = section.shadowRoot!.querySelector(
        'settings-local-credit-card-remove-confirmation-dialog');
    assertTrue(!!confirmationDialog);
    await whenAttributeIs(confirmationDialog.$.dialog, 'open', '');

    const closePromise = eventToPromise('close', confirmationDialog);

    const removeButton = confirmationDialog.$.remove;
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
        'settings-local-credit-card-remove-confirmation-dialog');
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

  test('verifyAddVirtualCardClicked', async function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
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

    assertFalse(section.$.menuAddVirtualCard.hidden);
    section.$.menuAddVirtualCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.addedVirtualCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyRemoveVirtualCardClicked', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = true;

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

    assertFalse(section.$.menuRemoveVirtualCard.hidden);
    section.$.menuRemoveVirtualCard.click();
    flush();

    const menu =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardSharedMenu');
    assertFalse(!!menu);
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

  test('verifyMigrationButtonNotShownIfMigrationNotEnabled', async function() {
    // Mock prerequisites are not met.
    loadTimeData.overrideValues({migrationEnabled: false});

    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonNotShownIfCreditCardDisabled', async function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    // Mock credit card save toggle is turned off by users.
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: false}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonNotShownIfNoCardIsMigratable', async function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    // Mock credit card is not valid.
    creditCard.metadata!.isMigratable = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonShown', async function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertFalse(section.$.migrateCreditCards.hidden);
  });

  test('verifyFIDOAuthToggleShownIfUserIsVerifiable', async function() {
    // Set |fidoAuthenticationAvailableForAutofill| to true.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertTrue(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleNotShownIfUserIsNotVerifiable', async function() {
    // Set |fidoAuthenticationAvailableForAutofill| to false.
    loadTimeData.overrideValues(
        {fidoAuthenticationAvailableForAutofill: false});
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    assertFalse(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleCheckedIfOptedIn', async function() {
    // Set FIDO auth pref value to true.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
          credit_card_enabled: {value: true},
          credit_card_fido_auth_enabled: {value: true},
        });
    assertTrue(section.shadowRoot!
                   .querySelector<SettingsToggleButtonElement>(
                       '#autofillCreditCardFIDOAuthToggle')!.checked);
  });

  test('verifyFIDOAuthToggleUncheckedIfOptedOut', async function() {
    // Set FIDO auth pref value to false.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
          credit_card_enabled: {value: true},
          credit_card_fido_auth_enabled: {value: false},
        });
    assertFalse(section.shadowRoot!
                    .querySelector<SettingsToggleButtonElement>(
                        '#autofillCreditCardFIDOAuthToggle')!.checked);
  });

  test('verifyUpiIdRow', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], ['vpa@indianbank'],
        /*prefValues=*/ {});
    const rowShadowRoot = getUPIRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot);
    assertEquals(
        rowShadowRoot.querySelector<HTMLElement>('#upiIdLabel')!.textContent,
        'vpa@indianbank');
  });

  test('verifyNoUpiId', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('verifyUpiIdCount', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const upiIds = ['vpa1@indianbank', 'vpa2@indianbank'];
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(upiIds.length, upiRows.length);
  });

  // Test that |showUpiIdSettings| controls showing UPI IDs in the page.
  test('verifyShowUpiIdSettings', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: false});

    const upiIds = ['vpa1@indianbank'];
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('CanMakePaymentToggle_RecordsMetrics', async function() {
    const testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    section.$.canMakePaymentToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');

    assertEquals(PrivacyElementInteractions.PAYMENT_METHOD, result);
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

  test(
      'verifyNoAddPaymentMethodsButtonIfPaymentPrefDisabled', async function() {
        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
            {credit_card_enabled: {value: false}});

        const addPaymentMethodsButton =
            section.shadowRoot!.querySelector<CrButtonElement>(
                '#addPaymentMethods');
        assertTrue(!!addPaymentMethodsButton);
        assertTrue(addPaymentMethodsButton.hidden);
      });

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
    // Can't be saved, because there's no value.
    assertTrue(saveButton.disabled);

    // Add invalid IBAN value.
    const valueInput = ibanDialog.$.valueInput;
    valueInput.value = '11112222333344445678';
    flush();
    // Can't be saved, because the value of IBAN is invalid.
    assertTrue(saveButton.disabled);

    // Add valid IBAN value.
    valueInput.value = 'FI1410093000123458';
    flush();
    // Can be saved, because the value of IBAN is valid.
    assertFalse(saveButton.disabled);

    const savePromise = eventToPromise('save-iban', ibanDialog);
    saveButton.click();
    const event = await savePromise;

    assertEquals(undefined, event.detail.guid);
    assertEquals('FI1410093000123458', event.detail.value);
    assertEquals('', event.detail.nickname);
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

  test('verifyRemoveIbanClicked', async function() {
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

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.removedIbans = 1;
    paymentsManager.assertExpectations(expectations);
  });

});
