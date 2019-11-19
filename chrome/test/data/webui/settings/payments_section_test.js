// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_payments_section', function() {
  suite('PaymentSectionUiTest', function() {
    test('testAutofillExtensionIndicator', function() {
      // Initializing with fake prefs
      const section = document.createElement('settings-payments-section');
      section.prefs = {
        autofill: {credit_card_enabled: {}, credit_card_fido_auth_enabled: {}}
      };
      document.body.appendChild(section);

      assertFalse(!!section.$$('#autofillExtensionIndicator'));
      section.set(
          'prefs.autofill.credit_card_enabled.extensionId', 'test-id-1');
      section.set(
          'prefs.autofill.credit_card_fido_auth_enabled.extensionId',
          'test-id-2');
      Polymer.dom.flush();

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
     * @param {!Object} prefValues
     * @return {!Object}
     */
    function createPaymentsSection(creditCards, prefValues) {
      // Override the PaymentsManagerImpl for testing.
      const paymentsManager = new TestPaymentsManager();
      paymentsManager.data.creditCards = creditCards;
      PaymentsManagerImpl.instance_ = paymentsManager;

      const section = document.createElement('settings-payments-section');
      section.prefs = {autofill: prefValues};
      document.body.appendChild(section);
      Polymer.dom.flush();

      return section;
    }

    /**
     * Creates the Edit Credit Card dialog.
     * @param {!chrome.autofillPrivate.CreditCardEntry} creditCardItem
     * @return {!Object}
     */
    function createCreditCardDialog(creditCardItem) {
      const section =
          document.createElement('settings-credit-card-edit-dialog');
      section.creditCard = creditCardItem;
      document.body.appendChild(section);
      Polymer.dom.flush();
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
    function getLocalAndServerListItems() {
      return document.body.querySelector('settings-payments-section')
          .$$('#creditCardList')
          .shadowRoot.querySelectorAll('settings-credit-card-list-entry');
    }

    /**
     * Returns the shadow root of the card row from the specified card list.
     * @param {!HTMLElement} cardList
     * @return {?HTMLElement}
     */
    function getCardRowShadowRoot(cardList) {
      const row = cardList.$$('settings-credit-card-list-entry');
      assertTrue(!!row);
      return row.shadowRoot;
    }

    test('verifyCreditCardCount', function() {
      const section =
          createPaymentsSection([], {credit_card_enabled: {value: true}});

      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(0, getLocalAndServerListItems().length);

      assertFalse(creditCardList.$$('#noCreditCardsLabel').hidden);
      assertTrue(creditCardList.$$('#creditCardsHeading').hidden);
      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertFalse(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardsDisabled', function() {
      const section =
          createPaymentsSection([], {credit_card_enabled: {value: false}});

      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertTrue(section.$$('#addCreditCard').hidden);
    });

    test('verifyCreditCardCount', function() {
      const creditCards = [
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry(),
      ];

      const section = createPaymentsSection(
          creditCards, {credit_card_enabled: {value: true}});
      const creditCardList = section.$$('#creditCardList');
      assertTrue(!!creditCardList);
      assertEquals(creditCards.length, getLocalAndServerListItems().length);

      assertTrue(creditCardList.$$('#noCreditCardsLabel').hidden);
      assertFalse(creditCardList.$$('#creditCardsHeading').hidden);
      assertFalse(section.$$('#autofillCreditCardToggle').disabled);
      assertFalse(section.$$('#addCreditCard').disabled);
    });

    test('verifyCreditCardFields', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      const section = createPaymentsSection([creditCard], {});
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertEquals(
          creditCard.metadata.summaryLabel,
          rowShadowRoot.querySelector('#creditCardLabel').textContent);
      assertEquals(
          creditCard.expirationMonth + '/' + creditCard.expirationYear,
          rowShadowRoot.querySelector('#creditCardExpiration')
              .textContent.trim());
    });

    test('verifyCreditCardRowButtonIsDropdownWhenLocal', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = true;
      const section = createPaymentsSection([creditCard], {});
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);
      const outlinkButton =
          rowShadowRoot.querySelector('cr-icon-button.icon-external');
      assertFalse(!!outlinkButton);
    });

    test('verifyCreditCardRowButtonIsOutlinkWhenRemote', function() {
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isLocal = false;
      const section = createPaymentsSection([creditCard], {});
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertFalse(!!menuButton);
      const outlinkButton =
          rowShadowRoot.querySelector('cr-icon-button.icon-external');
      assertTrue(!!outlinkButton);
    });

    test('verifyAddVsEditCreditCardTitle', function() {
      const newCreditCard = FakeDataMaker.emptyCreditCardEntry();
      const newCreditCardDialog = createCreditCardDialog(newCreditCard);
      const oldCreditCard = FakeDataMaker.creditCardEntry();
      const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

      assertNotEquals(oldCreditCardDialog.title_, newCreditCardDialog.title_);
      assertNotEquals('', newCreditCardDialog.title_);
      assertNotEquals('', oldCreditCardDialog.title_);

      // Wait for dialogs to open before finishing test.
      return Promise.all([
        test_util.whenAttributeIs(newCreditCardDialog.$.dialog, 'open', ''),
        test_util.whenAttributeIs(oldCreditCardDialog.$.dialog, 'open', ''),
      ]);
    });

    test('verifyExpiredCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // 2015 is over unless time goes wobbly.
      const twentyFifteen = 2015;
      creditCard.expirationYear = twentyFifteen.toString();

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
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
      const creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 25 years from now is unusual.
      const now = new Date();
      const farFutureYear = now.getFullYear() + 25;
      creditCard.expirationYear = farFutureYear.toString();

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals(
                now.getFullYear().toString(),
                yearOptions[0].textContent.trim());
            assertEquals(
                farFutureYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verifyVeryNormalCreditCardYear', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // Expiring 2 years from now is not unusual.
      const now = new Date();
      const nearFutureYear = now.getFullYear() + 2;
      creditCard.expirationYear = nearFutureYear.toString();
      const maxYear = now.getFullYear() + 19;

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            const yearOptions = creditCardDialog.$.year.options;

            assertEquals(
                now.getFullYear().toString(),
                yearOptions[0].textContent.trim());
            assertEquals(
                maxYear.toString(),
                yearOptions[yearOptions.length - 1].textContent.trim());
            assertEquals(
                creditCard.expirationYear, creditCardDialog.$.year.value);
          });
    });

    test('verify save disabled for expired credit card', function() {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();

      const now = new Date();
      creditCard.expirationYear = now.getFullYear() - 2;
      // works fine for January.
      creditCard.expirationMonth = now.getMonth() - 1;

      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            assertTrue(creditCardDialog.$.saveButton.disabled);
          });
    });

    test('verify save new credit card', function() {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialog(creditCard);

      return test_util.whenAttributeIs(
          creditCardDialog.$.dialog, 'open', '').
            then(function() {
              // Not expired, but still can't be saved, because there's no
              // name.
              assertTrue(creditCardDialog.$.expired.hidden);
              assertTrue(creditCardDialog.$.saveButton.disabled);

              // Add a name and trigger the on-input handler.
              creditCardDialog.set('creditCard.name', 'Jane Doe');
              creditCardDialog.onCreditCardNameOrNumberChanged_();
              Polymer.dom.flush();

              assertTrue(creditCardDialog.$.expired.hidden);
              assertFalse(creditCardDialog.$.saveButton.disabled);

              const savedPromise = test_util.eventToPromise('save-credit-card',
                  creditCardDialog);
              creditCardDialog.$.saveButton.click();
              return savedPromise;
            }).then(function(event) {
              assertEquals(creditCard.guid, event.detail.guid);
            });
    });

    test('verifyCancelCreditCardEdit', function(done) {
      const creditCard = FakeDataMaker.emptyCreditCardEntry();
      const creditCardDialog = createCreditCardDialog(creditCard);

      test_util.whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
          .then(function() {
            test_util.eventToPromise('save-credit-card', creditCardDialog)
              .then(function() {
                // Fail the test because the save event should not be called
                // when cancel is clicked.
                assertTrue(false);
                done();
              });

            test_util.eventToPromise('close', creditCardDialog)
              .then(function() {
                // Test is |done| in a timeout in order to ensure that
                // 'save-credit-card' is NOT fired after this test.
                window.setTimeout(done, 100);
              });

            creditCardDialog.$.cancelButton.click();
          });
    });

    test('verifyLocalCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      // When credit card is local, |isCached| will be undefined.
      creditCard.metadata.isLocal = true;
      creditCard.metadata.isCached = undefined;

      const section = createPaymentsSection([creditCard], {});
      assertEquals(1, getLocalAndServerListItems().length);

      // Local credit cards will show the overflow menu.
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      menuButton.click();
      Polymer.dom.flush();

      const menu = section.$.creditCardSharedMenu;

      // Menu should have 2 options.
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertFalse(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertTrue(menu.querySelector('#menuClearCreditCard').hidden);

      menu.close();
      Polymer.dom.flush();
    });

    test('verifyCachedCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      creditCard.metadata.isLocal = false;
      creditCard.metadata.isCached = true;

      const section = createPaymentsSection([creditCard], {});
      assertEquals(1, getLocalAndServerListItems().length);

      // Cached remote CCs will show overflow menu.
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
      const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
      assertTrue(!!menuButton);

      menuButton.click();
      Polymer.dom.flush();

      const menu = section.$.creditCardSharedMenu;

      // Menu should have 2 options.
      assertFalse(menu.querySelector('#menuEditCreditCard').hidden);
      assertTrue(menu.querySelector('#menuRemoveCreditCard').hidden);
      assertFalse(menu.querySelector('#menuClearCreditCard').hidden);

      menu.close();
      Polymer.dom.flush();
    });

    test('verifyNotCachedCreditCardMenu', function() {
      const creditCard = FakeDataMaker.creditCardEntry();

      creditCard.metadata.isLocal = false;
      creditCard.metadata.isCached = false;

      const section = createPaymentsSection([creditCard], {});
      assertEquals(1, getLocalAndServerListItems().length);

      // No overflow menu when not cached.
      const rowShadowRoot = getCardRowShadowRoot(section.$$('#creditCardList'));
      assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
      assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
    });

    test('verifyMigrationButtonNotShownIfMigrationNotEnabled', function() {
      // Mock prerequisites are not met.
      loadTimeData.overrideValues({migrationEnabled: false});

      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfCreditCardDisabled', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      // Mock credit card save toggle is turned off by users.
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: false}});

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonNotShownIfNoCardIsMigratable', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      // Mock credit card is not valid.
      creditCard.metadata.isMigratable = false;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      assertTrue(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyMigrationButtonShown', function() {
      // Add one migratable credit card.
      const creditCard = FakeDataMaker.creditCardEntry();
      creditCard.metadata.isMigratable = true;
      const section = createPaymentsSection(
          [creditCard], {credit_card_enabled: {value: true}});

      assertFalse(section.$$('#migrateCreditCards').hidden);
    });

    test('verifyFIDOAuthToggleShownIfUserIsVerifiable', function() {
      // Set |fidoAuthenticationAvailableForAutofill| to true.
      loadTimeData.overrideValues(
          {fidoAuthenticationAvailableForAutofill: true});
      addFakePlatformAuthenticator();
      const section =
          createPaymentsSection([], {credit_card_enabled: {value: true}});

      assertTrue(!!section.$$('#autofillCreditCardFIDOAuthToggle'));
    });

    test('verifyFIDOAuthToggleNotShownIfUserIsNotVerifiable', function() {
      // Set |fidoAuthenticationAvailableForAutofill| to false.
      loadTimeData.overrideValues(
          {fidoAuthenticationAvailableForAutofill: false});
      const section =
          createPaymentsSection([], {credit_card_enabled: {value: true}});
      assertFalse(!!section.$$('#autofillCreditCardFIDOAuthToggle'));
    });

    test('verifyFIDOAuthToggleCheckedIfOptedIn', function() {
      // Set FIDO auth pref value to true.
      loadTimeData.overrideValues(
          {fidoAuthenticationAvailableForAutofill: true});
      addFakePlatformAuthenticator();
      const section = createPaymentsSection([], {
        credit_card_enabled: {value: true},
        credit_card_fido_auth_enabled: {value: true}
      });
      assertTrue(section.$$('#autofillCreditCardFIDOAuthToggle').checked);
    });

    test('verifyFIDOAuthToggleUncheckedIfOptedOut', function() {
      // Set FIDO auth pref value to false.
      loadTimeData.overrideValues(
          {fidoAuthenticationAvailableForAutofill: true});
      addFakePlatformAuthenticator();
      const section = createPaymentsSection([], {
        credit_card_enabled: {value: true},
        credit_card_fido_auth_enabled: {value: false}
      });
      assertFalse(section.$$('#autofillCreditCardFIDOAuthToggle').checked);
    });
  });
});
