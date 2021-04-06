// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
import {createCreditCardEntry, createEmptyCreditCardEntry,TestPaymentsManager} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {TestMetricsBrowserProxy} from 'chrome://test/settings/test_metrics_browser_proxy.js';
import {eventToPromise, isVisible, whenAttributeIs} from 'chrome://test/test_util.m.js';

// clang-format on

suite('PaymentSectionUiTest', function() {
  test('testAutofillExtensionIndicator', function() {
    // Initializing with fake prefs
    const section = document.createElement('settings-payments-section');
    section.prefs = {
      autofill: {credit_card_enabled: {}, credit_card_fido_auth_enabled: {}}
    };
    document.body.appendChild(section);

    assertFalse(!!section.$$('#autofillExtensionIndicator'));
    section.set('prefs.autofill.credit_card_enabled.extensionId', 'test-id-1');
    section.set(
        'prefs.autofill.credit_card_fido_auth_enabled.extensionId',
        'test-id-2');
    flush();

    assertTrue(!!section.$$('#autofillExtensionIndicator'));
  });
});

suite('PaymentsSection', function() {
  setup(function() {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      migrationEnabled: true,
    });
  });

  /**
   * Creates the payments autofill section for the given list.
   * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
   * @param {!Array<!string>} upiIds
   * @param {!Object} prefValues
   * @return {!Object}
   */
  function createPaymentsSection(creditCards, upiIds, prefValues) {
    // Override the PaymentsManagerImpl for testing.
    const paymentsManager = new TestPaymentsManager();
    paymentsManager.data.creditCards = creditCards;
    paymentsManager.data.upiIds = upiIds;
    PaymentsManagerImpl.instance_ = paymentsManager;

    const section = document.createElement('settings-payments-section');
    section.prefs = {autofill: prefValues};
    document.body.appendChild(section);
    flush();

    return section;
  }

  /**
   * Creates the Edit Credit Card dialog.
   * @param {!chrome.autofillPrivate.CreditCardEntry} creditCardItem
   * @return {!Object}
   */
  function createCreditCardDialog(creditCardItem) {
    const section = document.createElement('settings-credit-card-edit-dialog');
    section.creditCard = creditCardItem;
    document.body.appendChild(section);
    flush();
    return section;
  }

  // Fakes the existence of a platform authenticator.
  function addFakePlatformAuthenticator() {
    if (!window.PublicKeyCredential) {
      window.PublicKeyCredential = {};
    }
    window.PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable =
        function() {
      return new Promise(callback => {
        callback(true);
      });
    };
  }


  /**
   * Returns an array containing the local and server credit card items.
   * @return {!Array<!chrome.autofillPrivate.CreditCardEntry>}
   */
  function getLocalAndServerCreditCardListItems() {
    return document.body.querySelector('settings-payments-section')
        .$$('#paymentsList')
        .shadowRoot.querySelectorAll('settings-credit-card-list-entry');
  }

  /**
   * Returns the shadow root of the card row from the specified list of
   * payment methods.
   * @param {!HTMLElement} paymentsList
   * @return {?HTMLElement}
   */
  function getCardRowShadowRoot(paymentsList) {
    const row = paymentsList.$$('settings-credit-card-list-entry');
    assertTrue(!!row);
    return row.shadowRoot;
  }

  /**
   * Returns the shadow root of the UPI ID row from the specified list of
   * payment methods.
   * @param {!HTMLElement} paymentsList
   * @return {?HTMLElement}
   */
  function getUPIRowShadowRoot(paymentsList) {
    const row = paymentsList.$$('settings-upi-id-list-entry');
    assertTrue(!!row);
    return row.shadowRoot;
  }

  test('verifyNoCreditCards', function() {
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    const creditCardList = section.$$('#paymentsList');
    assertTrue(!!creditCardList);
    assertEquals(0, getLocalAndServerCreditCardListItems().length);

    assertFalse(creditCardList.$$('#noPaymentMethodsLabel').hidden);
    assertTrue(creditCardList.$$('#creditCardsHeading').hidden);
    assertFalse(section.$$('#autofillCreditCardToggle').disabled);
    assertFalse(section.$$('#addCreditCard').disabled);
  });

  test('verifyCreditCardsDisabled', function() {
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: false}});

    assertFalse(section.$$('#autofillCreditCardToggle').disabled);
    assertTrue(section.$$('#addCreditCard').hidden);
  });

  test('verifyCreditCardCount', function() {
    const creditCards = [
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
    ];

    const section = createPaymentsSection(
        creditCards, /*upiIds=*/[], {credit_card_enabled: {value: true}});
    const creditCardList = section.$$('#paymentsList');
    assertTrue(!!creditCardList);
    assertEquals(
        creditCards.length, getLocalAndServerCreditCardListItems().length);

    assertTrue(creditCardList.$$('#noPaymentMethodsLabel').hidden);
    assertFalse(creditCardList.$$('#creditCardsHeading').hidden);
    assertFalse(section.$$('#autofillCreditCardToggle').disabled);
    assertFalse(section.$$('#addCreditCard').disabled);
  });

  test('verifyCreditCardFields', function() {
    const creditCard = createCreditCardEntry();
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$$('#paymentsList'));
    assertEquals(
        creditCard.metadata.summaryLabel,
        rowShadowRoot.querySelector('#creditCardLabel').textContent);
    assertEquals(
        creditCard.expirationMonth + '/' + creditCard.expirationYear,
        rowShadowRoot.querySelector('#creditCardExpiration')
            .textContent.trim());
  });

  test('verifyCreditCardRowButtonIsDropdownWhenLocal', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata.isLocal = true;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$$('#paymentsList'));
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertFalse(!!outlinkButton);
  });

  test('verifyCreditCardRowButtonIsOutlinkWhenRemote', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata.isLocal = false;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$$('#paymentsList'));
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertFalse(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
  });

  test('verifyAddVsEditCreditCardTitle', function() {
    const newCreditCard = createEmptyCreditCardEntry();
    const newCreditCardDialog = createCreditCardDialog(newCreditCard);
    const oldCreditCard = createCreditCardEntry();
    const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

    assertNotEquals(oldCreditCardDialog.title_, newCreditCardDialog.title_);
    assertNotEquals('', newCreditCardDialog.title_);
    assertNotEquals('', oldCreditCardDialog.title_);

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
          const yearOptions = creditCardDialog.$.year.options;

          assertEquals('2015', yearOptions[0].textContent.trim());
          assertEquals(
              maxYear.toString(),
              yearOptions[yearOptions.length - 1].textContent.trim());
          assertEquals(
              creditCard.expirationYear, creditCardDialog.$.year.value);
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
          const yearOptions = creditCardDialog.$.year.options;

          assertEquals(
              now.getFullYear().toString(), yearOptions[0].textContent.trim());
          assertEquals(
              farFutureYear.toString(),
              yearOptions[yearOptions.length - 1].textContent.trim());
          assertEquals(
              creditCard.expirationYear, creditCardDialog.$.year.value);
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
          const yearOptions = creditCardDialog.$.year.options;

          assertEquals(
              now.getFullYear().toString(), yearOptions[0].textContent.trim());
          assertEquals(
              maxYear.toString(),
              yearOptions[yearOptions.length - 1].textContent.trim());
          assertEquals(
              creditCard.expirationYear, creditCardDialog.$.year.value);
        });
  });

  test('verify save new credit card', function() {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          // Not expired, but still can't be saved, because there's no
          // name.
          const expiredError = creditCardDialog.$$('#expired-error');
          assertEquals('hidden', getComputedStyle(expiredError).visibility);
          assertTrue(creditCardDialog.$.saveButton.disabled);

          // Add a name.
          creditCardDialog.set('creditCard.name', 'Jane Doe');
          flush();

          assertEquals('hidden', getComputedStyle(expiredError).visibility);
          assertFalse(creditCardDialog.$.saveButton.disabled);

          const savedPromise =
              eventToPromise('save-credit-card', creditCardDialog);
          creditCardDialog.$.saveButton.click();
          return savedPromise;
        })
        .then(function(event) {
          assertEquals(creditCard.guid, event.detail.guid);
        });
  });

  test('verifyCancelCreditCardEdit', function(done) {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    whenAttributeIs(creditCardDialog.$.dialog, 'open', '').then(function() {
      eventToPromise('save-credit-card', creditCardDialog).then(function() {
        // Fail the test because the save event should not be called
        // when cancel is clicked.
        assertTrue(false);
        done();
      });

      eventToPromise('close', creditCardDialog).then(function() {
        // Test is |done| in a timeout in order to ensure that
        // 'save-credit-card' is NOT fired after this test.
        window.setTimeout(done, 100);
      });

      creditCardDialog.$.cancelButton.click();
    });
  });

  test('verifyLocalCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    // When credit card is local, |isCached| will be undefined.
    creditCard.metadata.isLocal = true;
    creditCard.metadata.isCached = undefined;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Local credit cards will show the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$$('#paymentsList'));
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    const menu = section.$.creditCardSharedMenu;

    // Menu should have 2 options.
    assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
    assertFalse(menu.querySelector('#menuRemoveCreditCard').hidden);
    assertTrue(menu.querySelector('#menuClearCreditCard').hidden);

    menu.close();
    flush();
  });

  test('verifyCachedCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata.isLocal = false;
    creditCard.metadata.isCached = true;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Cached remote CCs will show overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$$('#paymentsList'));
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    const menu = section.$.creditCardSharedMenu;

    // Menu should have 2 options.
    assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
    assertTrue(menu.querySelector('#menuRemoveCreditCard').hidden);
    assertFalse(menu.querySelector('#menuClearCreditCard').hidden);

    menu.close();
    flush();
  });

  test('verifyNotCachedCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata.isLocal = false;
    creditCard.metadata.isCached = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // No overflow menu when not cached.
    const rowShadowRoot = getCardRowShadowRoot(section.$$('#paymentsList'));
    assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
  });

  test('verifyMigrationButtonNotShownIfMigrationNotEnabled', function() {
    // Mock prerequisites are not met.
    loadTimeData.overrideValues({migrationEnabled: false});

    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata.isMigratable = true;
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: true}});

    assertTrue(section.$$('#migrateCreditCards').hidden);
  });

  test('verifyMigrationButtonNotShownIfCreditCardDisabled', function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata.isMigratable = true;
    // Mock credit card save toggle is turned off by users.
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: false}});

    assertTrue(section.$$('#migrateCreditCards').hidden);
  });

  test('verifyMigrationButtonNotShownIfNoCardIsMigratable', function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    // Mock credit card is not valid.
    creditCard.metadata.isMigratable = false;
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: true}});

    assertTrue(section.$$('#migrateCreditCards').hidden);
  });

  test('verifyMigrationButtonShown', function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata.isMigratable = true;
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: true}});

    assertFalse(section.$$('#migrateCreditCards').hidden);
  });

  test('verifyFIDOAuthToggleShownIfUserIsVerifiable', function() {
    // Set |fidoAuthenticationAvailableForAutofill| to true.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertTrue(!!section.$$('#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleNotShownIfUserIsNotVerifiable', function() {
    // Set |fidoAuthenticationAvailableForAutofill| to false.
    loadTimeData.overrideValues(
        {fidoAuthenticationAvailableForAutofill: false});
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    assertFalse(!!section.$$('#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleCheckedIfOptedIn', function() {
    // Set FIDO auth pref value to true.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = createPaymentsSection(/*creditCards=*/[], /*upiIds=*/[], {
      credit_card_enabled: {value: true},
      credit_card_fido_auth_enabled: {value: true}
    });
    assertTrue(section.$$('#autofillCreditCardFIDOAuthToggle').checked);
  });

  test('verifyFIDOAuthToggleUncheckedIfOptedOut', function() {
    // Set FIDO auth pref value to false.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = createPaymentsSection(/*creditCards=*/[], /*upiIds=*/[], {
      credit_card_enabled: {value: true},
      credit_card_fido_auth_enabled: {value: false}
    });
    assertFalse(section.$$('#autofillCreditCardFIDOAuthToggle').checked);
  });

  test('verifyUpiIdRow', function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = createPaymentsSection(
        /*creditCards=*/[], ['vpa@indianbank'], /*prefValues=*/ {});
    const rowShadowRoot = getUPIRowShadowRoot(section.$$('#paymentsList'));
    assertTrue(!!rowShadowRoot);
    assertEquals(
        rowShadowRoot.querySelector('#upiIdLabel').textContent,
        'vpa@indianbank');
  });

  test('verifyNoUpiId', function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const paymentsList = section.$$('#paymentsList');
    const upiRows =
        paymentsList.shadowRoot.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('verifyUpiIdCount', function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const upiIds = ['vpa1@indianbank', 'vpa2@indianbank'];
    const section = createPaymentsSection(
        /*creditCards=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$$('#paymentsList');
    const upiRows =
        paymentsList.shadowRoot.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(upiIds.length, upiRows.length);
  });

  // Test that |showUpiIdSettings| controls showing UPI IDs in the page.
  test('verifyShowUpiIdSettings', function() {
    loadTimeData.overrideValues({showUpiIdSettings: false});

    const upiIds = ['vpa1@indianbank'];
    const section = createPaymentsSection(
        /*creditCards=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$$('#paymentsList');
    const upiRows =
        paymentsList.shadowRoot.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('CanMakePaymentToggle_RecordsMetrics', async function() {
    const testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;

    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    section.$$('#canMakePaymentToggle').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');

    assertEquals(PrivacyElementInteractions.PAYMENT_METHOD, result);
  });
});
