// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {CrButtonElement, loadTimeData, MetricsBrowserProxyImpl, PrivacyElementInteractions, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isMac, isWindows} from 'chrome://resources/js/platform.js';

import {createCreditCardEntry, TestPaymentsManager} from './passwords_and_autofill_fake_data.js';
import {createPaymentsSection, getLocalAndServerCreditCardListItems, getDefaultExpectations, getCardRowShadowRoot} from './payments_section_utils.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

import {isVisible} from 'chrome://webui-test/test_util.js';


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
      deviceAuthAvailable: true,
    });
  });

  // Fakes the existence of a platform authenticator.
  function addFakePlatformAuthenticator() {
    (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
        .setIsUserVerifyingPlatformAuthenticatorAvailable(true);
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

  test(
      'verifyMandatoryAuthToggleShownIfBiometricIsAvailableAndAutofillToggleIsOn',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: false},
            });

        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        if (isMac || isWindows) {
          assertTrue(!!mandatoryAuthToggle);
        } else {
          assertFalse(!!mandatoryAuthToggle);
        }
      });

  test(
      'verifyMandatoryAuthToggleShownIfBiometricIsNotAvailableAndMandatoryAuthToggleIsOn',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: false});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: true},
            });

        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        if (isMac || isWindows) {
          assertTrue(!!mandatoryAuthToggle);
        } else {
          assertFalse(!!mandatoryAuthToggle);
        }
      });

  test(
      'verifyMandatoryAuthToggleShownIfBiometricIsAvailableAndMandatoryAuthToggleIsOn',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: true},
            });

        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        if (isMac || isWindows) {
          assertTrue(!!mandatoryAuthToggle);
        } else {
          assertFalse(!!mandatoryAuthToggle);
        }
      });

  test(
      'verifyMandatoryAuthToggleNotShownIfBiometricIsNotAvailableAndMandatoryAuthToggleIsOff',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: false});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: false},
            });

        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');
        assertFalse(!!mandatoryAuthToggle);
      });

  test(
      'verifyMandatoryAuthToggleNotShownIfBiometricIsAvailableAndAutofillToggleIsOffAndMandatoryAuthToggleIsOn',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: false},
              payment_methods_mandatory_reauth: {value: true},
            });

        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');
        assertFalse(!!mandatoryAuthToggle);
      });

  test(
      'verifyMandatoryAuthToggleNotShownIfBiometricIsAvailableAndAutofillToggleIsOff',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: false},
              payment_methods_mandatory_reauth: {value: false},
            });

        assertFalse(section.$.autofillCreditCardToggle.disabled);
        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        assertFalse(!!mandatoryAuthToggle);
      });

  test(
      'verifyMandatoryAuthToggleNotShownIfMandatoryAuthToggleIsOffAndAutofillToggleIsOff',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: false});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: false},
              payment_methods_mandatory_reauth: {value: false},
            });

        assertFalse(section.$.autofillCreditCardToggle.disabled);
        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        assertFalse(!!mandatoryAuthToggle);
      });

  test(
      'verifyMandatoryAuthToggleNotShownIfMandatoryAuthToggleIsOnAndAutofillToggleIsOff',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: false});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: false},
              payment_methods_mandatory_reauth: {value: true},
            });

        assertFalse(section.$.autofillCreditCardToggle.disabled);
        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        assertFalse(!!mandatoryAuthToggle);
      });

  test(
      'verifyMandatoryAuthToggleDoesTriggerUserAuthWhenClicked',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: false},
            });
        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        if (isMac || isWindows) {
          const paymentsManagerProxy =
              PaymentsManagerImpl.getInstance() as TestPaymentsManager;
          const expectations = getDefaultExpectations();

          assertTrue(!!mandatoryAuthToggle);
          mandatoryAuthToggle.click();
          expectations.authenticateUserAndFlipMandatoryAuthToggle = 1;
          paymentsManagerProxy.assertExpectations(expectations);
        } else {
          assertFalse(!!mandatoryAuthToggle);
        }
      });

  test(
      'verifyMandatoryAuthToggleDoesNotTriggersUserAuthWhenNotClicked',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: false},
            });
        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');
        const paymentsManagerProxy =
            PaymentsManagerImpl.getInstance() as TestPaymentsManager;
        const expectations = getDefaultExpectations();

        if (isMac || isWindows) {
          assertTrue(!!mandatoryAuthToggle);
        } else {
          assertFalse(!!mandatoryAuthToggle);
        }
        paymentsManagerProxy.assertExpectations(expectations);
      });

  test('verifyEditLocalCardTriggersUserAuth', async function() {
    loadTimeData.overrideValues({deviceAuthAvailable: true});

    const section = await createPaymentsSection(
        [createCreditCardEntry()], /*ibans=*/[], /*upiIds=*/[], {
          credit_card_enabled: {value: true},
          payment_methods_mandatory_reauth: {value: true},
        });

    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));

    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertTrue(isVisible(section.$.menuEditCreditCard));
    section.$.menuEditCreditCard.click();
    flush();

    const paymentsManagerProxy =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.authenticateUserToEditLocalCard = 1;
    paymentsManagerProxy.assertExpectations(expectations);
  });
});
