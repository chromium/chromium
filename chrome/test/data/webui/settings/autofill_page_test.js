// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_autofill_page', function() {
  suite('PasswordsAndForms', function() {
    /**
     * Creates a new passwords and forms element.
     * @return {!Object}
     */
    function createAutofillElement(prefsElement) {
      const element = document.createElement('settings-autofill-page');
      element.prefs = prefsElement.prefs;
      document.body.appendChild(element);

      element.$$('dom-if[route-path="/passwords"]').if = true;
      element.$$('dom-if[route-path="/payments"]').if = true;
      element.$$('dom-if[route-path="/addresses"]').if = true;
      Polymer.dom.flush();
      return element;
    }

    /**
     * @pram {boolean} autofill Whether autofill is enabled or not.
     * @param {boolean} passwords Whether passwords are enabled or not.
     * @return {!Promise<!Element>} The |prefs| element.
     */
    function createPrefs(autofill, passwords) {
      return new Promise(function(resolve) {
        CrSettingsPrefs.deferInitialization = true;
        const prefs = document.createElement('settings-prefs');
        prefs.initialize(new settings.FakeSettingsPrivate([
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
            key: 'profile.password_manager_leak_detection',
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
     * @param {!Element} prefs The prefs element.
     */
    function destroyPrefs(prefs) {
      CrSettingsPrefs.resetForTesting();
      CrSettingsPrefs.deferInitialization = false;
      prefs.resetForTesting();
    }

    /**
     * Creates PasswordManagerExpectations with the values expected after first
     * creating the element.
     * @return {!PasswordManagerExpectations}
     */
    function basePasswordExpectations() {
      const expected = new PasswordManagerExpectations();
      expected.requested.passwords = 1;
      expected.requested.exceptions = 1;
      expected.listening.passwords = 1;
      expected.listening.exceptions = 1;
      return expected;
    }

    /**
     * Creates AutofillManagerExpectations with the values expected after first
     * creating the element.
     * @return {!AutofillManagerExpectations}
     */
    function baseAutofillExpectations() {
      const expected = new AutofillManagerExpectations();
      expected.requestedAddresses = 1;
      expected.listeningAddresses = 1;
      return expected;
    }

    /**
     * Creates PaymentsManagerExpectations with the values expected after first
     * creating the element.
     * @return {!PaymentsManagerExpectations}
     */
    function basePaymentsExpectations() {
      const expected = new PaymentsManagerExpectations();
      expected.requestedCreditCards = 1;
      expected.listeningCreditCards = 1;
      return expected;
    }

    let passwordManager;
    let autofillManager;
    let paymentsManager;


    setup(function() {
      PolymerTest.clearBody();

      // Override the PasswordManagerImpl for testing.
      passwordManager = new TestPasswordManagerProxy();
      PasswordManagerImpl.instance_ = passwordManager;

      // Override the AutofillManagerImpl for testing.
      autofillManager = new TestAutofillManager();
      AutofillManagerImpl.instance_ = autofillManager;

      // Override the PaymentsManagerImpl for testing.
      paymentsManager = new TestPaymentsManager();
      PaymentsManagerImpl.instance_ = paymentsManager;
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
        Polymer.dom.flush();

        passwordsExpectations.listening.passwords = 0;
        passwordsExpectations.listening.exceptions = 0;
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

        const list =
            [FakeDataMaker.passwordEntry(), FakeDataMaker.passwordEntry()];

        passwordManager.lastCallback.addSavedPasswordListChangedListener(list);
        Polymer.dom.flush();

        assertDeepEquals(
            list,
            element.$$('#passwordSection')
                .savedPasswords.map(entry => entry.entry));

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

        const list =
            [FakeDataMaker.exceptionEntry(), FakeDataMaker.exceptionEntry()];
        passwordManager.lastCallback.addExceptionListChangedListener(list);
        Polymer.dom.flush();

        assertEquals(list, element.$$('#passwordSection').passwordExceptions);

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

        const addressList =
            [FakeDataMaker.addressEntry(), FakeDataMaker.addressEntry()];
        const cardList =
            [FakeDataMaker.creditCardEntry(), FakeDataMaker.creditCardEntry()];
        autofillManager.lastCallback.setPersonalDataManagerListener(
            addressList, cardList);
        Polymer.dom.flush();

        assertEquals(addressList, element.$$('#autofillSection').addresses);

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

        const addressList =
            [FakeDataMaker.addressEntry(), FakeDataMaker.addressEntry()];
        const cardList =
            [FakeDataMaker.creditCardEntry(), FakeDataMaker.creditCardEntry()];
        paymentsManager.lastCallback.setPersonalDataManagerListener(
            addressList, cardList);
        Polymer.dom.flush();

        assertEquals(cardList, element.$$('#paymentsSection').creditCards);

        // The callback is coming from the manager, so the element shouldn't
        // have additional calls to the manager after the base expectations.
        passwordManager.assertExpectations(basePasswordExpectations());
        autofillManager.assertExpectations(baseAutofillExpectations());
        paymentsManager.assertExpectations(basePaymentsExpectations());

        destroyPrefs(prefs);
      });
    });
  });

  suite('PasswordsUITest', function() {
    /** @type {SettingsAutofillPageElement} */
    let autofillPage = null;
    /** @type {settings.OpenWindowProxy} */
    let openWindowProxy = null;

    suiteSetup(function() {
      // Forces navigation to Google Password Manager to be off by default.
      loadTimeData.overrideValues({
        navigateToGooglePasswordManager: false,
      });
    });

    setup(function() {
      openWindowProxy = new TestOpenWindowProxy();
      settings.OpenWindowProxyImpl.instance_ = openWindowProxy;

      PolymerTest.clearBody();
      autofillPage = document.createElement('settings-autofill-page');
      autofillPage.prefs = {
        profile: {
          password_manager_leak_detection: {},
        },
      };
      document.body.appendChild(autofillPage);

      Polymer.dom.flush();
    });

    teardown(function() {
      autofillPage.remove();
    });

    test('Google Password Manager Off', function() {
      assertTrue(!!autofillPage.$$('#passwordManagerButton'));
      autofillPage.$$('#passwordManagerButton').click();
      Polymer.dom.flush();

      assertEquals(settings.getCurrentRoute(), settings.routes.PASSWORDS);
    });

    test('Google Password Manager On', function() {
      // Hardcode this value so that the test is independent of the production
      // implementation that might include additional query parameters.
      const googlePasswordManagerUrl = 'https://passwords.google.com';

      loadTimeData.overrideValues({
        navigateToGooglePasswordManager: true,
        googlePasswordManagerUrl: googlePasswordManagerUrl,
      });

      assertTrue(!!autofillPage.$$('#passwordManagerButton'));
      autofillPage.$$('#passwordManagerButton').click();
      Polymer.dom.flush();

      return openWindowProxy.whenCalled('openURL').then(url => {
        assertEquals(googlePasswordManagerUrl, url);
      });
    });
  });
});
