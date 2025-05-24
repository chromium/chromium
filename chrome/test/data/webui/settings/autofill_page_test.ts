// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import type {SettingsAutofillSectionElement, SettingsPaymentsSectionElement} from 'chrome://settings/lazy_load.js';
import {AutofillManagerImpl, PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {resetRouterForTesting} from 'chrome://settings/settings.js';
import type {CrLinkRowElement, SettingsAutofillPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, OpenWindowProxyImpl, PasswordManagerImpl, SettingsPluralStringProxyImpl, PasswordManagerPage} from 'chrome://settings/settings.js';
import {assertEquals, assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {AutofillManagerExpectations, createAddressEntry, createCreditCardEntry, createIbanEntry, createPayOverTimeIssuerEntry, PaymentsManagerExpectations, STUB_USER_ACCOUNT_INFO, TestAutofillManager, TestPaymentsManager} from './autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

suite('PasswordsAndForms', function() {
  /**
   * Creates a new passwords and forms element.
   */
  function createAutofillElement(prefsElement: SettingsPrefsElement):
      SettingsAutofillPageElement {
    const element = document.createElement('settings-autofill-page');
    element.prefs = prefsElement.prefs;
    document.body.appendChild(element);
    flush();

    // Force-render all subppages.
    element.shadowRoot!.querySelector<DomIf>(
                           'dom-if[route-path="/payments"]')!.if = true;
    element.shadowRoot!.querySelector<DomIf>(
                           'dom-if[route-path="/addresses"]')!.if = true;
    flush();
    return element;
  }

  /**
   * @param autofill Whether autofill is enabled or not.
   * @param passwords Whether passwords are enabled or not.
   */
  function createPrefs(
      autofill: boolean, passwords: boolean): Promise<SettingsPrefsElement> {
    return new Promise(function(resolve) {
      CrSettingsPrefs.deferInitialization = true;
      const prefs = document.createElement('settings-prefs');
      prefs.initialize(new FakeSettingsPrivate([
        {
          key: 'autofill.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: autofill,
        },
        {
          key: 'autofill.profile_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        {
          key: 'autofill.credit_card_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        {
          key: 'credentials_enable_service',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: passwords,
        },
        {
          key: 'credentials_enable_autosignin',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        {
          key: 'payments.can_make_payment_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        {
          key: 'autofill.payment_methods_mandatory_reauth',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,

        },
      ]));

      CrSettingsPrefs.initialized.then(function() {
        resolve(prefs);
      });
    });
  }

  /**
   * Cleans up prefs so tests can continue to run.
   * @param prefs The prefs element.
   */
  function destroyPrefs(prefs: SettingsPrefsElement) {
    CrSettingsPrefs.resetForTesting();
    CrSettingsPrefs.deferInitialization = false;
    prefs.resetForTesting();
  }

  /**
   * Creates AutofillManagerExpectations with the values expected after first
   * creating the element.
   */
  function baseAutofillExpectations(): AutofillManagerExpectations {
    const expected = new AutofillManagerExpectations();
    expected.requestedAddresses = 1;
    expected.listeningAddresses = 1;
    return expected;
  }

  /**
   * Creates PaymentsManagerExpectations with the values expected after first
   * creating the element.
   */
  function basePaymentsExpectations(): PaymentsManagerExpectations {
    const expected = new PaymentsManagerExpectations();
    expected.requestedCreditCards = 1;
    expected.listeningCreditCards = 1;
    expected.requestedIbans = 1;
    expected.requestedPayOverTimeIssuers = 1;
    return expected;
  }

  let autofillManager: TestAutofillManager;
  let paymentsManager: TestPaymentsManager;
  let prefs: SettingsPrefsElement;
  let element: SettingsAutofillPageElement;

  setup(async function() {
    loadTimeData.overrideValues({
      shouldShowPayOverTimeSettings: true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Override the AutofillManagerImpl for testing.
    autofillManager = new TestAutofillManager();
    AutofillManagerImpl.setInstance(autofillManager);

    // Override the PaymentsManagerImpl for testing.
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);

    prefs = await createPrefs(true, true);
    element = createAutofillElement(prefs);
  });

  teardown(function() {
    destroyPrefs(prefs);
  });

  test('baseLoadAndRemove', function() {
    const autofillExpectations = baseAutofillExpectations();
    autofillManager.assertExpectations(autofillExpectations);

    const paymentsExpectations = basePaymentsExpectations();
    paymentsManager.assertExpectations(paymentsExpectations);

    element.remove();
    flush();

    autofillExpectations.listeningAddresses = 0;
    autofillManager.assertExpectations(autofillExpectations);

    paymentsExpectations.listeningCreditCards = 0;
    paymentsManager.assertExpectations(paymentsExpectations);
  });

  test('autofillAiButtonHidden', async function() {
    loadTimeData.overrideValues({
      showAutofillAiControl: false,
    });
    // Recreate the element with the new `loadTimeData`.
    element.remove();
    element = createAutofillElement(prefs);
    // Make sure that the button is not created asynchronously.
    await flushTasks();
    // Assert that the button is not visible.
    const autofillAiManagerButton =
        element.shadowRoot!.querySelector<CrLinkRowElement>(
            '#autofillAiManagerButton');
    assertTrue(autofillAiManagerButton === null);
  });

  test('AutofillAIButtonVisible', function() {
    loadTimeData.overrideValues({
      showAutofillAiControl: true,
    });
    // Recreate the element with the new `loadTimeData`.
    element.remove();
    element = createAutofillElement(prefs);
    // Assert that the button is visible.
    const autofillAiManagerButton =
        element.shadowRoot!.querySelector<CrLinkRowElement>(
            '#autofillAiManagerButton');
    assertTrue(autofillAiManagerButton !== null);
  });

  test('loadAddressesAsync', function() {
    const addressList = [createAddressEntry(), createAddressEntry()];
    const cardList = [createCreditCardEntry(), createCreditCardEntry()];
    const ibanList = [createIbanEntry(), createIbanEntry()];
    const payOverTimeIssuerList =
        [createPayOverTimeIssuerEntry(), createPayOverTimeIssuerEntry()];
    const accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    autofillManager.lastCallback.setPersonalDataManagerListener!
        (addressList, cardList, ibanList, payOverTimeIssuerList, accountInfo);
    flush();

    assertDeepEquals(
        addressList,
        element.shadowRoot!
            .querySelector<SettingsAutofillSectionElement>(
                '#autofillSection')!.addresses);

    // The callback is coming from the manager, so the element shouldn't
    // have additional calls to the manager after the base expectations.
    autofillManager.assertExpectations(baseAutofillExpectations());
    paymentsManager.assertExpectations(basePaymentsExpectations());
  });

  test('loadPaymentMethodsAsync', function() {
    const addressList = [createAddressEntry(), createAddressEntry()];
    const cardList = [createCreditCardEntry(), createCreditCardEntry()];
    const ibanList = [createIbanEntry(), createIbanEntry()];
    const issuerList =
        [createPayOverTimeIssuerEntry(), createPayOverTimeIssuerEntry()];
    const accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    paymentsManager.lastCallback.setPersonalDataManagerListener!
        (addressList, cardList, ibanList, issuerList, accountInfo);
    flush();

    const paymentsSection =
        element.shadowRoot!.querySelector<SettingsPaymentsSectionElement>(
            '#paymentsSection');
    assertTrue(!!paymentsSection);
    assertEquals(
        cardList, paymentsSection.creditCards,
        'The cardList should be loaded into the paymentsSection');
    assertEquals(
        ibanList, paymentsSection.ibans,
        'The ibanList should be loaded into the paymentsSection');
    assertEquals(
        issuerList, paymentsSection.payOverTimeIssuers,
        'The issuerList should be loaded into the paymentsSection');

    // The callback is coming from the manager, so the element shouldn't
    // have additional calls to the manager after the base expectations.
    autofillManager.assertExpectations(baseAutofillExpectations());
    paymentsManager.assertExpectations(basePaymentsExpectations());
  });
});

function createAutofillPageSection() {
  // Create a passwords-section to use for testing.
  const autofillPage = document.createElement('settings-autofill-page');
  autofillPage.prefs = {
    profile: {
      password_manager_leak_detection: {},
    },
    credentials_enable_service: {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      value: false,
    },
  };
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  document.body.appendChild(autofillPage);
  flush();
  return autofillPage;
}

suite('PasswordsUITest', function() {
  let autofillPage: SettingsAutofillPageElement;
  let openWindowProxy: TestOpenWindowProxy;
  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);

    autofillPage = createAutofillPageSection();
  });

  teardown(function() {
    autofillPage.remove();
  });

  test('Clicking Password Manager item', async function() {
    resetRouterForTesting();

    const autofillSection = createAutofillPageSection();
    assertTrue(autofillSection.$.passwordManagerButton.external);

    autofillSection.$.passwordManagerButton.click();
    const param = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, param);
  });
});
