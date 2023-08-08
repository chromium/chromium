// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {CrButtonElement, loadTimeData, MetricsBrowserProxyImpl, PrivacyElementInteractions, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createCreditCardEntry, TestPaymentsManager} from './autofill_fake_data.js';
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
      virtualCardEnrollmentEnabled: true,
      showIbansSettings: true,
      deviceAuthAvailable: true,
      autofillEnablePaymentsMandatoryReauth: true,
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

  // Scenario1:
  // FIDO toggle shown- True
  // User Verified- True
  // Mandatory Reauth Flag- False
  test('FidoAuthScenario1', async function() {
    loadTimeData.overrideValues({
      fidoAuthenticationAvailableForAutofill: true,
      autofillEnablePaymentsMandatoryReauth: false,
    });
    addFakePlatformAuthenticator();
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertTrue(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  // Scenario2:
  // FIDO toggle shown- False
  // User Verified- True
  // Mandatory Reauth Flag- True
  test('FidoAuthScenario2', async function() {
    loadTimeData.overrideValues({
      fidoAuthenticationAvailableForAutofill: true,
      autofillEnablePaymentsMandatoryReauth: true,
    });
    addFakePlatformAuthenticator();
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertFalse(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  // Scenario3:
  // FIDO toggle shown- False
  // User Verified- False
  // Mandatory Reauth Flag- False
  test('FidoAuthScenario3', async function() {
    loadTimeData.overrideValues(
        {fidoAuthenticationAvailableForAutofill: false});
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    assertFalse(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleCheckedIfOptedIn', async function() {
    loadTimeData.overrideValues({
      fidoAuthenticationAvailableForAutofill: true,
      autofillEnablePaymentsMandatoryReauth: false,
    });
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
    loadTimeData.overrideValues({
      fidoAuthenticationAvailableForAutofill: true,
      autofillEnablePaymentsMandatoryReauth: false,
    });
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

  /**
   * The following tests deal with the Mandatory reauth feature. There are
   * various conditions that can change the reauth toggle. Here are those
   * conditions along with their shorthands to be used in the tests-
   *    1. Mandatory reauth feature flag = flag
   *    2. Biometric or Screen lock = device unlock
   *    3. Autofill toggle = autofill
   *    4. Mandatory reauth toggle = reauth
   *
   * There is another comment below to denote the end of the reauth tests.
   */

  test(
      'verifyReauthShownIfDeviceUnlockIsAvailableAndAutofillIsOn',
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

        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        assertFalse(mandatoryAuthToggle.checked);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthShownIfDeviceUnlockIsAvailableAndReauthIsOn',
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
        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        assertTrue(mandatoryAuthToggle.checked);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthNotShownIfDeviceUnlockIsAvailableAndReauthIsOnButFlagIsOff',
      async function() {
        loadTimeData.overrideValues({
          deviceAuthAvailable: true,
          autofillEnablePaymentsMandatoryReauth: false,
        });

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: true},
            });

        assertFalse(
            !!section.shadowRoot!.querySelector('#mandatoryAuthToggle'));
      });

  test(
      'verifyReauthDisabledIfDeviceUnlockIsNotAvailableAndReauthIsOn',
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

        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        assertTrue(mandatoryAuthToggle.disabled);
        assertTrue(mandatoryAuthToggle.checked);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthIsDisabledIfDeviceUnlockIsNotAvailableAndReauthIsOffAndAutofillIsOn',
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

        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        assertTrue(mandatoryAuthToggle.disabled);
        assertFalse(mandatoryAuthToggle.checked);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthDisabledIfDeviceUnlockIsAvailableAndReauthIsOnAndAutofillIsOff',
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

        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        assertTrue(mandatoryAuthToggle.disabled);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthDisabledIfDeviceUnlockIsAvailableAndReauthIsOffAndAutofillIsOff',
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

        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        assertTrue(mandatoryAuthToggle.disabled);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthDoesTriggerUserAuthWhenClicked', async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: true});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
              credit_card_enabled: {value: true},
              payment_methods_mandatory_reauth: {value: false},
            });

        const mandatoryAuthToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mandatoryAuthToggle');

        // <if expr="is_win or is_macosx">
        const expectations = getDefaultExpectations();
        assertTrue(!!mandatoryAuthToggle);
        mandatoryAuthToggle.click();
        expectations.authenticateUserAndFlipMandatoryAuthToggle = 1;
        (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
            .assertExpectations(expectations);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
      });

  test(
      'verifyReauthDoesNotTriggersUserAuthWhenNotClicked', async function() {
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

        // <if expr="is_win or is_macosx">
        assertTrue(!!mandatoryAuthToggle);
        // </if>
        // <if expr="not is_win and not is_macosx">
        assertFalse(!!mandatoryAuthToggle);
        // </if>
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

  // --------- End of Reauth Tests ---------

  test('verifyCvvStorageToggleIsShown', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], {
          credit_card_enabled: {value: true},
        });
    const cvcStorageToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#cvcStorageToggle');

    assertTrue(!!cvcStorageToggle);
    assertEquals(
        cvcStorageToggle.subLabelWithLink,
        loadTimeData.getString('enableCvcStorageDeleteDataSublabel'));
  });
});
