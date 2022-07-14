// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {DomIf, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManagerImpl, PasswordsSectionElement, PaymentsManagerImpl, SettingsAutofillSectionElement, SettingsPaymentsSectionElement} from 'chrome://settings/lazy_load.js';
import {buildRouter, Router, routes} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, MultiStorePasswordUiEntry, OpenWindowProxyImpl, PasswordManagerImpl, SettingsAutofillPageElement, SettingsPluralStringProxyImpl, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {SettingsRoutes} from 'chrome://settings/settings_routes.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {FakeSettingsPrivate} from './fake_settings_private.js';
import {AutofillManagerExpectations, createAddressEntry, createCreditCardEntry, createExceptionEntry, createMultiStorePasswordEntry, createPasswordEntry, PaymentsManagerExpectations, TestAutofillManager, TestPaymentsManager} from './passwords_and_autofill_fake_data.js';
import {makeCompromisedCredential} from './passwords_and_autofill_fake_data.js';
import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {PasswordManagerExpectations,TestPasswordManagerProxy} from './test_password_manager_proxy.js';

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

    element.shadowRoot!.querySelector<DomIf>(
                           'dom-if[route-path="/passwords"]')!.if = true;
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
                       ]) as unknown as typeof chrome.settingsPrivate);

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
   * Creates PasswordManagerExpectations with the values expected after first
   * creating the element.
   */
  function basePasswordExpectations(): PasswordManagerExpectations {
    const expected = new PasswordManagerExpectations();
    expected.requested.passwords = 1;
    expected.requested.exceptions = 1;
    expected.requested.accountStorageOptInState = 1;
    expected.listening.passwords = 1;
    expected.listening.exceptions = 1;
    expected.listening.accountStorageOptInState = 1;
    return expected;
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
    return expected;
  }

  let passwordManager: TestPasswordManagerProxy;
  let autofillManager: TestAutofillManager;
  let paymentsManager: TestPaymentsManager;


  setup(async function() {
    document.body.innerHTML = '';

    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    // Override the AutofillManagerImpl for testing.
    autofillManager = new TestAutofillManager();
    AutofillManagerImpl.setInstance(autofillManager);

    // Override the PaymentsManagerImpl for testing.
    paymentsManager = new TestPaymentsManager();
    PaymentsManagerImpl.setInstance(paymentsManager);
  });

  test('baseLoadAndRemove', function() {
    return createPrefs(true, true).then(function(prefs) {
      const element = createAutofillElement(prefs);

      const passwordsExpectations = basePasswordExpectations();
      passwordManager.assertExpectations(passwordsExpectations);

      const autofillExpectations = baseAutofillExpectations();
      autofillManager.assertExpectations(autofillExpectations);

      const paymentsExpectations = basePaymentsExpectations();
      paymentsManager.assertExpectations(paymentsExpectations);

      element.remove();
      flush();

      passwordsExpectations.listening.passwords = 0;
      passwordsExpectations.listening.exceptions = 0;
      passwordsExpectations.listening.accountStorageOptInState = 0;
      passwordManager.assertExpectations(passwordsExpectations);

      autofillExpectations.listeningAddresses = 0;
      autofillManager.assertExpectations(autofillExpectations);

      paymentsExpectations.listeningCreditCards = 0;
      paymentsManager.assertExpectations(paymentsExpectations);

      destroyPrefs(prefs);
    });
  });

  test('loadPasswordsAsync', function() {
    return createPrefs(true, true).then(function(prefs) {
      const element = createAutofillElement(prefs);

      const list = [
        createPasswordEntry({url: 'one.com', username: 'user1', id: 0}),
        createPasswordEntry({url: 'two.com', username: 'user1', id: 1}),
      ];

      passwordManager.lastCallback.addSavedPasswordListChangedListener!(list);
      flush();

      assertDeepEquals(
          list.map(entry => new MultiStorePasswordUiEntry(entry)),
          element.shadowRoot!
              .querySelector<PasswordsSectionElement>(
                  '#passwordSection')!.savedPasswords);

      // The callback is coming from the manager, so the element shouldn't
      // have additional calls to the manager after the base expectations.
      passwordManager.assertExpectations(basePasswordExpectations());
      autofillManager.assertExpectations(baseAutofillExpectations());
      paymentsManager.assertExpectations(basePaymentsExpectations());

      destroyPrefs(prefs);
    });
  });

  test('loadExceptionsAsync', function() {
    return createPrefs(true, true).then(function(prefs) {
      const element = createAutofillElement(prefs);

      const list = [
        createExceptionEntry({url: 'one.com', id: 0}),
        createExceptionEntry({url: 'two.com', id: 1}),
      ];
      passwordManager.lastCallback.addExceptionListChangedListener!(list);
      flush();

      assertDeepEquals(
          list,
          element.shadowRoot!
              .querySelector<PasswordsSectionElement>(
                  '#passwordSection')!.passwordExceptions);

      // The callback is coming from the manager, so the element shouldn't
      // have additional calls to the manager after the base expectations.
      passwordManager.assertExpectations(basePasswordExpectations());
      autofillManager.assertExpectations(baseAutofillExpectations());
      paymentsManager.assertExpectations(basePaymentsExpectations());

      destroyPrefs(prefs);
    });
  });

  test('loadAddressesAsync', function() {
    return createPrefs(true, true).then(function(prefs) {
      const element = createAutofillElement(prefs);

      const addressList = [createAddressEntry(), createAddressEntry()];
      const cardList = [createCreditCardEntry(), createCreditCardEntry()];
      autofillManager.lastCallback.setPersonalDataManagerListener!
          (addressList, cardList);
      flush();

      assertEquals(
          addressList,
          element.shadowRoot!
              .querySelector<SettingsAutofillSectionElement>(
                  '#autofillSection')!.addresses);

      // The callback is coming from the manager, so the element shouldn't
      // have additional calls to the manager after the base expectations.
      passwordManager.assertExpectations(basePasswordExpectations());
      autofillManager.assertExpectations(baseAutofillExpectations());
      paymentsManager.assertExpectations(basePaymentsExpectations());

      destroyPrefs(prefs);
    });
  });

  test('loadCreditCardsAsync', function() {
    return createPrefs(true, true).then(function(prefs) {
      const element = createAutofillElement(prefs);

      const addressList = [createAddressEntry(), createAddressEntry()];
      const cardList = [createCreditCardEntry(), createCreditCardEntry()];
      paymentsManager.lastCallback.setPersonalDataManagerListener!
          (addressList, cardList);
      flush();

      assertEquals(
          cardList,
          element.shadowRoot!
              .querySelector<SettingsPaymentsSectionElement>(
                  '#paymentsSection')!.creditCards);

      // The callback is coming from the manager, so the element shouldn't
      // have additional calls to the manager after the base expectations.
      passwordManager.assertExpectations(basePasswordExpectations());
      autofillManager.assertExpectations(baseAutofillExpectations());
      paymentsManager.assertExpectations(basePaymentsExpectations());

      destroyPrefs(prefs);
    });
  });
});

function createAutofillPageSection() {
  // Create a passwords-section to use for testing.
  const autofillPage = document.createElement('settings-autofill-page');
  autofillPage.prefs = {
    profile: {
      password_manager_leak_detection: {},
    },
  };
  document.body.innerHTML = '';
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

  test('Compromised Credential', async function() {
    // Check if sublabel is empty
    assertEquals(
        '',
        autofillPage.shadowRoot!
            .querySelector<HTMLElement>(
                '#passwordManagerSubLabel')!.innerText.trim());

    // Simulate one compromised password
    const leakedPasswords = [
      makeCompromisedCredential(
          'google.com', 'jdoerrie',
          chrome.passwordsPrivate.CompromiseType.LEAKED),
    ];
    passwordManager.data.leakedCredentials = leakedPasswords;

    // create autofill page with leaked credentials
    autofillPage = createAutofillPageSection();

    await passwordManager.whenCalled('getCompromisedCredentials');
    await pluralString.whenCalled('getPluralString');

    // With compromised credentials sublabel should have text
    assertNotEquals(
        '',
        autofillPage.shadowRoot!
            .querySelector<HTMLElement>(
                '#passwordManagerSubLabel')!.innerText.trim());
  });

  test('Credential urls is used in the subpage header', async function() {
    const SHOWN_URL = 'www.google.com';
    loadTimeData.overrideValues({enablePasswordViewPage: true});
    Router.resetInstanceForTesting(buildRouter());
    routes.PASSWORD_VIEW =
        (Router.getInstance().getRoutes() as SettingsRoutes).PASSWORD_VIEW;
    const autofillSection = createAutofillPageSection();

    Router.getInstance().navigateTo(routes.PASSWORD_VIEW);
    await flushTasks();
    const subpage =
        autofillSection.shadowRoot!.querySelector('settings-subpage');

    autofillSection.credential =
        createMultiStorePasswordEntry({url: SHOWN_URL, deviceId: 1});
    flush();

    assertTrue(!!subpage);
    assertEquals(`http://${SHOWN_URL}/login`, subpage.faviconSiteUrl);
    assertEquals(SHOWN_URL, subpage.pageTitle);
  });
});
