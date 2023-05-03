// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomIf, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManagerImpl, PasswordsSectionElement, PasswordListItemElement, PaymentsManagerImpl, SettingsAutofillSectionElement, SettingsPaymentsSectionElement} from 'chrome://settings/lazy_load.js';
import {buildRouter, Router, routes} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, OpenWindowProxyImpl, PasswordManagerImpl, SettingsAutofillPageElement, SettingsPluralStringProxyImpl, SettingsPrefsElement, SettingsRoutes, PasswordManagerPage} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {AutofillManagerExpectations, createAddressEntry, createCreditCardEntry, createExceptionEntry, createIbanEntry, createPasswordEntry, PaymentsManagerExpectations, STUB_USER_ACCOUNT_INFO, TestAutofillManager, TestPaymentsManager} from './passwords_and_autofill_fake_data.js';
import {makeInsecureCredential} from './passwords_and_autofill_fake_data.js';
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
    flush();

    // Force-render all subppages.

    if (!loadTimeData.getBoolean('enableNewPasswordManagerPage')) {
      element.shadowRoot!
          .querySelector<DomIf>('dom-if[route-path="/passwords"]')!.if = true;
    }

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
    expected.requestedIbans = 1;
    return expected;
  }

  let passwordManager: TestPasswordManagerProxy;
  let autofillManager: TestAutofillManager;
  let paymentsManager: TestPaymentsManager;


  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

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
          list,
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
      const ibanList = [createIbanEntry(), createIbanEntry()];
      const accountInfo = {
        ...STUB_USER_ACCOUNT_INFO,
        isSyncEnabledForAutofillProfiles: true,
      };
      autofillManager.lastCallback.setPersonalDataManagerListener!
          (addressList, cardList, ibanList, accountInfo);
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
      const ibanList = [createIbanEntry(), createIbanEntry()];
      const accountInfo = {
        ...STUB_USER_ACCOUNT_INFO,
        isSyncEnabledForAutofillProfiles: true,
      };
      paymentsManager.lastCallback.setPersonalDataManagerListener!
          (addressList, cardList, ibanList, accountInfo);
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

  test('loadIbansAsync', function() {
    return createPrefs(true, true).then(function(prefs) {
      const element = createAutofillElement(prefs);

      const addressList = [createAddressEntry(), createAddressEntry()];
      const cardList = [createCreditCardEntry(), createCreditCardEntry()];
      const ibanList = [createIbanEntry(), createIbanEntry()];
      const accountInfo = {
        ...STUB_USER_ACCOUNT_INFO,
        isSyncEnabledForAutofillProfiles: true,
      };
      paymentsManager.lastCallback.setPersonalDataManagerListener!
          (addressList, cardList, ibanList, accountInfo);
      flush();

      assertEquals(
          ibanList,
          element.shadowRoot!
              .querySelector<SettingsPaymentsSectionElement>(
                  '#paymentsSection')!.ibans);

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

  test('Compromised Credential', async function() {
    // Check if sublabel is empty
    assertEquals(
        '',
        autofillPage.shadowRoot!
            .querySelector<HTMLElement>(
                '#passwordManagerSubLabel')!.innerText.trim());

    // Simulate one compromised password
    const leakedPasswords = [
      makeInsecureCredential(
          'google.com', 'jdoerrie',
          [chrome.passwordsPrivate.CompromiseType.LEAKED]),
    ];
    passwordManager.data.insecureCredentials = leakedPasswords;

    // create autofill page with leaked credentials
    autofillPage = createAutofillPageSection();

    await passwordManager.whenCalled('getInsecureCredentials');
    await pluralString.whenCalled('getPluralString');

    // With compromised credentials sublabel should have text
    assertNotEquals(
        '',
        autofillPage.shadowRoot!
            .querySelector<HTMLElement>(
                '#passwordManagerSubLabel')!.innerText.trim());
  });

  ['passwords-section', 'password-view'].forEach(
      testCase => test(
          `After "password-view-page-requested" event from ${
              testCase}, password view page is visible after auth`,
          async function() {
            const SHOWN_URL = 'www.google.com';
            loadTimeData.overrideValues({enablePasswordViewPage: true});
            Router.resetInstanceForTesting(buildRouter());
            routes.PASSWORD_VIEW =
                (Router.getInstance().getRoutes() as SettingsRoutes)
                    .PASSWORD_VIEW;
            const autofillSection = createAutofillPageSection();

            const entry = createPasswordEntry({url: SHOWN_URL, id: 1});

            if (testCase === 'passwords-section') {
              autofillSection.shadowRoot!
                  .querySelector<DomIf>('dom-if[route-path="/passwords"]')!.if =
                  true;
            } else {
              autofillSection.shadowRoot!
                  .querySelector<DomIf>(
                      'dom-if[route-path="/passwords/view"]')!.if = true;
            }
            await flushTasks();
            const dispatchingElement =
                autofillSection.shadowRoot!.querySelector(testCase)!;
            const eventDetail = document.createElement('password-list-item');
            eventDetail.entry = entry;

            passwordManager.setRequestCredentialsDetailsResponse(entry);

            dispatchingElement.dispatchEvent(
                new CustomEvent('password-view-page-requested', {
                  bubbles: true,
                  composed: true,
                  detail: eventDetail,
                }));
            await flushTasks();

            assertEquals(
                routes.PASSWORD_VIEW, Router.getInstance().getCurrentRoute());
            assertDeepEquals(entry, autofillSection.credential);

            const subpage =
                [...autofillSection.shadowRoot!.querySelectorAll(
                     'settings-subpage')]
                    .find(
                        (element: HTMLElement) =>
                            element.classList.contains('iron-selected'));
            assertTrue(!!subpage);
            assertEquals(`http://${SHOWN_URL}/login`, subpage.faviconSiteUrl!);
            assertEquals(SHOWN_URL, subpage.pageTitle!);
          }));

  test(
      `After "password-view-page-requested" event with invalid id,
      password main page is opened`,
      async function() {
        loadTimeData.overrideValues({enablePasswordViewPage: true});
        Router.resetInstanceForTesting(buildRouter());
        routes.PASSWORD_VIEW =
            (Router.getInstance().getRoutes() as SettingsRoutes).PASSWORD_VIEW;
        const autofillSection = createAutofillPageSection();

        autofillSection.shadowRoot!
            .querySelector<DomIf>('dom-if[route-path="/passwords/view"]')!.if =
            true;
        await flushTasks();
        Router.getInstance().setCurrentRoute(
            routes.PASSWORD_VIEW, new URLSearchParams('id=123'), false);
        await flushTasks();

        const eventDetail = {entry: {id: 123}} as unknown as
            PasswordListItemElement;
        autofillSection.shadowRoot!.querySelector('password-view')!
            .dispatchEvent(new CustomEvent('password-view-page-requested', {
              bubbles: true,
              composed: true,
              detail: eventDetail,
            }));
        await flushTasks();

        assertEquals(routes.PASSWORDS, Router.getInstance().getCurrentRoute());
        assertFalse(!!autofillSection.credential);
      });

  test('New Password Manager UI enabled', async function() {
    // Enable new Password Manager UI.
    loadTimeData.overrideValues({enableNewPasswordManagerPage: true});
    Router.resetInstanceForTesting(buildRouter());

    const autofillSection = createAutofillPageSection();
    assertTrue(autofillSection.$.passwordManagerButton.external);

    autofillSection.$.passwordManagerButton.click();
    const param = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.PASSWORDS, param);
  });

  test('New Password Manager UI disabled', async function() {
    // Enable new Password Manager UI.
    loadTimeData.overrideValues({enableNewPasswordManagerPage: false});
    Router.resetInstanceForTesting(buildRouter());

    const autofillSection = createAutofillPageSection();

    assertFalse(autofillSection.$.passwordManagerButton.external);

    autofillSection.$.passwordManagerButton.click();
    assertEquals(routes.PASSWORDS, Router.getInstance().getCurrentRoute());
  });
});
