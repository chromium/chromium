// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSimpleConfirmationDialogElement} from 'chrome://settings/lazy_load.js';
import {GOOGLE_PAY_HELP_URL, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CvcDeletionUserAction, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createCreditCardEntry} from './autofill_fake_data.js';
import {createPaymentsSection, getLocalAndServerCreditCardListItems, getDefaultExpectations, getCardRowShadowRoot} from './payments_section_utils.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isVisible, whenAttributeIs} from 'chrome://webui-test/test_util.js';


// clang-format on

suite('PaymentSectionUiTest', function() {
  test('testAutofillExtensionIndicator', function() {
    // Initializing with fake prefs
    const section = document.createElement('settings-payments-section');
    section.prefs = {
      autofill: {credit_card_enabled: {}},
    };
    document.body.appendChild(section);

    assertFalse(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
    section.set('prefs.autofill.credit_card_enabled.extensionId', 'test-id-1');
    flush();

    assertTrue(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
  });
});

suite('PaymentsSection', function() {
  let openWindowProxy: TestOpenWindowProxy;
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    loadTimeData.overrideValues({
      migrationEnabled: true,
      showIbansSettings: true,
      deviceAuthAvailable: true,
    });
  });

  test('verifyNoCreditCards', async function() {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], {credit_card_enabled: {value: true}});

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
        /*creditCards=*/[], /*ibans=*/[],
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
        creditCards, /*ibans=*/[], {credit_card_enabled: {value: true}});
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
        [creditCard], /*ibans=*/[], {credit_card_enabled: {value: true}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonNotShownIfCreditCardDisabled', async function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    // Mock credit card save toggle is turned off by users.
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], {credit_card_enabled: {value: false}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonNotShownIfNoCardIsMigratable', async function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    // Mock credit card is not valid.
    creditCard.metadata!.isMigratable = false;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], {credit_card_enabled: {value: true}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonShown', async function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    const section = await createPaymentsSection(
        [creditCard], /*ibans=*/[], {credit_card_enabled: {value: true}});

    assertFalse(section.$.migrateCreditCards.hidden);
  });

  test('CanMakePaymentToggle_RecordsMetrics', async function() {
    const testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*prefValues=*/ {});

    section.$.canMakePaymentToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');

    assertEquals(PrivacyElementInteractions.PAYMENT_METHOD, result);
  });

  test(
      'verifyNoAddPaymentMethodsButtonIfPaymentPrefDisabled', async function() {
        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[],
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
            /*creditCards=*/[], /*ibans=*/[], {
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
            /*creditCards=*/[], /*ibans=*/[], {
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
      'verifyReauthDisabledIfDeviceUnlockIsNotAvailableAndReauthIsOn',
      async function() {
        loadTimeData.overrideValues({deviceAuthAvailable: false});

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], {
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
            /*creditCards=*/[], /*ibans=*/[], {
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
            /*creditCards=*/[], /*ibans=*/[], {
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
            /*creditCards=*/[], /*ibans=*/[], {
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
            /*creditCards=*/[], /*ibans=*/[], {
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
            /*creditCards=*/[], /*ibans=*/[], {
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

    const section =
        await createPaymentsSection([createCreditCardEntry()], /*ibans=*/[], {
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
    expectations.getLocalCard = 1;
    paymentsManagerProxy.assertExpectations(expectations);
  });

  // --------- End of Reauth Tests ---------

  test('verifyCvcStorageToggleIsShown', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], {
          credit_card_enabled: {value: true},
        });
    const cvcStorageToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#cvcStorageToggle');

    assertTrue(!!cvcStorageToggle);
    assertEquals(
        loadTimeData.getString('enableCvcStorageSublabel'),
        cvcStorageToggle.subLabelWithLink.toString());
    assertEquals(
        loadTimeData.getString('enableCvcStorageAriaLabelForNoCvcSaved'),
        cvcStorageToggle.ariaLabel);
  });

  test('verifyCvcStorageToggleSublabelWithDeletionIsShown', async function() {
    loadTimeData.overrideValues({
      cvcStorageAvailable: true,
    });

    const creditCard = createCreditCardEntry();
    creditCard.cvc = '•••';
    const section = await createPaymentsSection(
        /*creditCards=*/[creditCard], /*ibans=*/[], {
          credit_card_enabled: {value: true},
        });
    const cvcStorageToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#cvcStorageToggle');

    assertTrue(!!cvcStorageToggle);
    assertEquals(
        loadTimeData.getString('enableCvcStorageDeleteDataSublabel'),
        cvcStorageToggle.subLabelWithLink.toString());
    assertEquals(
        loadTimeData.getString('enableCvcStorageLabel'),
        cvcStorageToggle.ariaLabel);
  });

  test(
      'verifyCvcStorageToggleSublabelWithoutDeletionIsShown', async function() {
        loadTimeData.overrideValues({
          cvcStorageAvailable: true,
        });

        const creditCard = createCreditCardEntry();
        const section = await createPaymentsSection(
            /*creditCards=*/[creditCard], /*ibans=*/[], {
              credit_card_enabled: {value: true},
            });
        const cvcStorageToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#cvcStorageToggle');

        assertTrue(!!cvcStorageToggle);
        assertEquals(
            loadTimeData.getString('enableCvcStorageSublabel'),
            cvcStorageToggle.subLabelWithLink.toString());
      });

  // Test to verify if bulk delete is triggered or not based on how user
  // interacts with the deletion dialog window.
  [true, false].forEach(shouldTriggerBulkDelete => {
    test(
        `verifyBulkDeleteCvcIsTriggered_${shouldTriggerBulkDelete}`,
        async function() {
          loadTimeData.overrideValues({
            cvcStorageAvailable: true,
          });
          const testMetricsBrowserProxy = new TestMetricsBrowserProxy();
          MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

          const creditCard = createCreditCardEntry();
          creditCard.cvc = '•••';
          const section = await createPaymentsSection(
              /*creditCards=*/[creditCard], /*ibans=*/[], {
                credit_card_enabled: {value: true},
              });

          const cvcStorageToggle =
              section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#cvcStorageToggle');
          assertTrue(!!cvcStorageToggle);
          assertEquals(
              loadTimeData.getString('enableCvcStorageDeleteDataSublabel'),
              cvcStorageToggle.subLabelWithLink.toString());

          const cvcStorageToggleSublabelLink =
              cvcStorageToggle.$.labelWrapper
                  .querySelector('#sub-label-text-with-link')!.querySelector(
                      'a');
          assertTrue(isVisible(cvcStorageToggleSublabelLink));
          cvcStorageToggleSublabelLink!.click();
          flush();

          const bulkDeletionDialog =
              section.shadowRoot!
                  .querySelector<SettingsSimpleConfirmationDialogElement>(
                      '#bulkDeleteCvcConfirmDialog');
          assertTrue(!!bulkDeletionDialog);
          await whenAttributeIs(bulkDeletionDialog.$.dialog, 'open', '');

          if (shouldTriggerBulkDelete) {
            bulkDeletionDialog.$.confirm.click();
          } else {
            bulkDeletionDialog.$.cancel.click();
          }
          flush();

          // Wait for the dialog close event to propagate to the PaymentManager.
          await eventToPromise('close', bulkDeletionDialog);

          const paymentsManagerProxy =
              PaymentsManagerImpl.getInstance() as TestPaymentsManager;
          const expectations = getDefaultExpectations();

          assertEquals(2, testMetricsBrowserProxy.getCallCount('recordAction'));
          assertEquals(
              CvcDeletionUserAction.HYPERLINK_CLICKED,
              testMetricsBrowserProxy.getArgs('recordAction')[0]);
          if (shouldTriggerBulkDelete) {
            expectations.bulkDeleteAllCvcs = 1;
            assertEquals(
                CvcDeletionUserAction.DIALOG_ACCEPTED,
                testMetricsBrowserProxy.getArgs('recordAction')[1]);
          } else {
            assertEquals(
                CvcDeletionUserAction.DIALOG_CANCELLED,
                testMetricsBrowserProxy.getArgs('recordAction')[1]);
          }
          paymentsManagerProxy.assertExpectations(expectations);
        });
  });

  test('verifyCardBenefitsToggleIsShown', async function() {
    loadTimeData.overrideValues({
      autofillCardBenefitsAvailable: true,
    });

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], {
          credit_card_enabled: {value: true},
        });
    const cardBenefitsToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#cardBenefitsToggle');

    assertTrue(!!cardBenefitsToggle);
    assertEquals(
        loadTimeData.getString('cardBenefitsLabel'),
        cardBenefitsToggle.label.toString());
    assertEquals(
        loadTimeData.getString('cardBenefitsToggleSublabel'),
        cardBenefitsToggle.subLabelWithLink.toString());
  });

  test(
      'verifyCardBenefitsToggleIsNotShownWhenCardBenefitsFlagIsOff',
      async function() {
        loadTimeData.overrideValues({
          autofillCardBenefitsAvailable: false,
        });

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], {
              credit_card_enabled: {value: true},
            });

        assertFalse(!!section.shadowRoot!.querySelector('#cardBenefitsToggle'));
      });

  test(
      'verifyCardBenefitsToggleIsDisabledWhenCreditCardEnabledIsOff',
      async function() {
        loadTimeData.overrideValues({
          autofillCardBenefitsAvailable: true,
        });

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], {
              credit_card_enabled: {value: false},
            });
        const cardBenefitsToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#cardBenefitsToggle');

        assertTrue(!!cardBenefitsToggle);
        assertTrue(cardBenefitsToggle.disabled);
      });

  test('verifyCardBenefitsToggleSublabelLinkClickOpensUrl', async function() {
    loadTimeData.overrideValues({
      autofillCardBenefitsAvailable: true,
    });

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], {
          credit_card_enabled: {value: true},
        });
    const cardBenefitsToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#cardBenefitsToggle');
    assertTrue(!!cardBenefitsToggle);

    const link = cardBenefitsToggle.shadowRoot!.querySelector('a');
    assertTrue(!!link);
    link.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(GOOGLE_PAY_HELP_URL, url);
  });

  test('verifyCardBenefitsPrefIsFalseWhenToggleIsOff', async function() {
    loadTimeData.overrideValues({
      autofillCardBenefitsAvailable: true,
    });

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], {
          credit_card_enabled: {value: true},
          payment_card_benefits: {value: true},
        });
    const cardBenefitsToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#cardBenefitsToggle');
    assertTrue(!!cardBenefitsToggle);
    assertTrue(cardBenefitsToggle.checked);

    cardBenefitsToggle.click();

    assertFalse(cardBenefitsToggle.checked);
    assertFalse(cardBenefitsToggle.pref!.value);
  });
});
