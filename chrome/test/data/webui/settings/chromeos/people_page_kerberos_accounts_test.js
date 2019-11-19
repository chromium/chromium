// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

cr.define('settings_people_page_kerberos_accounts', function() {
  // List of fake accounts.
  const testAccounts = [
    {
      principalName: 'user@REALM',
      config: 'config1',
      isSignedIn: true,
      isActive: true,
      isManaged: false,
      passwordWasRemembered: false,
      pic: 'pic',
      validForDuration: '1 lightyear',
    },
    {
      principalName: 'user2@REALM2',
      config: 'config2',
      isSignedIn: false,
      isActive: false,
      isManaged: false,
      passwordWasRemembered: true,
      pic: 'pic2',
      validForDuration: 'zero googolseconds',
    },
    {
      principalName: 'user3@REALM3',
      config: 'config3',
      isSignedIn: false,
      isActive: false,
      isManaged: true,
      passwordWasRemembered: true,
      pic: 'pic2',
      validForDuration: 'one over inf seconds',
    }
  ];

  /** @implements {settings.KerberosAccountsBrowserProxy} */
  class TestKerberosAccountsBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'getAccounts',
        'addAccount',
        'removeAccount',
        'validateConfig',
        'setAsActiveAccount',
      ]);

      // Simulated error from an addAccount call.
      this.addAccountError = settings.KerberosErrorType.kNone;

      // Simulated error from a validateConfig call.
      this.validateConfigResult = {
        error: settings.KerberosErrorType.kNone,
        errorInfo: {code: settings.KerberosConfigErrorCode.kNone}
      };
    }

    /** @override */
    getAccounts() {
      this.methodCalled('getAccounts');
      return Promise.resolve(testAccounts);
    }

    /** @override */
    addAccount(
        principalName, password, rememberPassword, config, allowExisting) {
      this.methodCalled(
          'addAccount',
          [principalName, password, rememberPassword, config, allowExisting]);
      return Promise.resolve(this.addAccountError);
    }

    /** @override */
    removeAccount(account) {
      this.methodCalled('removeAccount', account);
      return Promise.resolve(settings.KerberosErrorType.kNone);
    }

    /** @override */
    validateConfig(account) {
      this.methodCalled('validateConfig', account);
      return Promise.resolve(this.validateConfigResult);
    }

    /** @override */
    setAsActiveAccount(account) {
      this.methodCalled('setAsActiveAccount', account);
    }
  }

  // Tests for the Kerberos Accounts settings page.
  suite('KerberosAccountsTests', function() {
    let browserProxy = null;
    let kerberosAccounts = null;
    let accountList = null;

    // Account indices (to help readability).
    const Account = {
      FIRST: 0,
      SECOND: 1,
      THIRD: 1,
    };

    // Indices of 'More Actions' buttons.
    const MoreActions = {
      REFRESH_NOW: 0,
      SET_AS_ACTIVE_ACCOUNT: 1,
      REMOVE_ACCOUNT: 2,
    };

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      createDialog();
    });

    teardown(function() {
      kerberosAccounts.remove();
    });

    function createDialog() {
      if (kerberosAccounts) {
        kerberosAccounts.remove();
      }

      kerberosAccounts = document.createElement('settings-kerberos-accounts');
      document.body.appendChild(kerberosAccounts);

      accountList = kerberosAccounts.$$('#account-list');
      assertTrue(!!accountList);
    }

    function clickMoreActions(accountIndex, moreActionsIndex) {
      // Click 'More actions' for the given account.
      kerberosAccounts.shadowRoot
          .querySelectorAll('.more-actions')[accountIndex]
          .click();
      // Click on the given action.
      kerberosAccounts.$$('cr-action-menu')
          .querySelectorAll('button')[moreActionsIndex]
          .click();
    }

    // Can be used to wait for event |eventName| on |element|, e.g.
    //   await onEvent(addDialog, 'close');
    function onEvent(element, eventName) {
      return new Promise(function(resolve) {
        element.addEventListener(eventName, function callback() {
          element.removeEventListener(eventName, callback);
          resolve();
        });
      });
    }

    test('AccountListIsPopulatedAtStartup', async () => {
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      // The test accounts were added in |getAccounts()| mock above.
      assertEquals(testAccounts.length, accountList.items.length);
    });

    test('AccountListSignedInSignedOutLabels', async () => {
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      accountList =
          kerberosAccounts.shadowRoot.querySelectorAll('.account-list-item');
      assertEquals(testAccounts.length, accountList.length);

      // Show 'Valid for <duration>' for accounts that are signed in.
      let signedIn = accountList[0].querySelector('.signed-in');
      let signedOut = accountList[0].querySelector('.signed-out');
      assertTrue(testAccounts[0].isSignedIn);
      assertFalse(signedIn.hidden);
      assertTrue(signedOut.hidden);
      assertEquals(
          'Valid for ' + testAccounts[0].validForDuration, signedIn.innerText);

      // Show 'Expired' for accounts that are not signed in.
      signedIn = accountList[1].querySelector('.signed-in');
      signedOut = accountList[1].querySelector('.signed-out');
      assertFalse(testAccounts[1].isSignedIn);
      assertTrue(signedIn.hidden);
      assertFalse(signedOut.hidden);
      assertEquals('Expired', signedOut.innerText);
    });

    test('AddAccount', function() {
      assertTrue(!kerberosAccounts.$$('kerberos-add-account-dialog'));
      kerberosAccounts.$$('#add-account-button').click();
      Polymer.dom.flush();
      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      assertEquals('', addDialog.$.username.value);
    });

    test('ReauthenticateAccount', async () => {
      // Wait until accounts are loaded.
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();

      // The kerberos-add-account-dialog shouldn't be open yet.
      assertTrue(!kerberosAccounts.$$('kerberos-add-account-dialog'));

      // Click "Sign-In" on an existing account.
      // Note that both accounts have a reauth button, but the first one is
      // hidden, so click the second one (clicking a hidden button works, but
      // it feels weird).
      kerberosAccounts.shadowRoot
          .querySelectorAll('.reauth-button')[Account.SECOND]
          .click();
      Polymer.dom.flush();

      // Now the kerberos-add-account-dialog should be open with preset
      // username.
      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      assertEquals(
          testAccounts[Account.SECOND].principalName,
          addDialog.$.username.value);
    });

    // Appending '?kerberos_reauth=<principal>' to the URL opens the reauth
    // dialog for that account.
    test('HandleReauthQueryParameter', async () => {
      const principal_name = testAccounts[Account.FIRST].principalName;
      const params = new URLSearchParams;
      params.append('kerberos_reauth', principal_name);
      settings.navigateTo(settings.routes.KERBEROS_ACCOUNTS, params);

      // The flushTasks is necessary since the kerberos_reauth param would
      // otherwise be handled AFTER the callback below is executed.
      await browserProxy.whenCalled('getAccounts');
      await test_util.flushTasks();
      Polymer.dom.flush();
      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      assertEquals(principal_name, addDialog.$.username.value);
    });

    test('RefreshNow', async () => {
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      clickMoreActions(Account.FIRST, MoreActions.REFRESH_NOW);
      Polymer.dom.flush();

      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      assertEquals(
          testAccounts[Account.FIRST].principalName,
          addDialog.$.username.value);
    });

    test('RefreshAccountShowsToast', async () => {
      const toast = kerberosAccounts.$$('#account-toast');
      assertTrue(!!toast);
      assertFalse(toast.open);

      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      clickMoreActions(Account.FIRST, MoreActions.REFRESH_NOW);
      Polymer.dom.flush();

      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      addDialog.$$('.action-button').click();
      Polymer.dom.flush();

      await onEvent(addDialog, 'close');
      await test_util.flushTasks();
      Polymer.dom.flush();
      assertTrue(toast.open);
      assertTrue(kerberosAccounts.$$('#account-toast-label')
                     .innerHTML.includes('refreshed'));
    });

    test('RemoveAccount', async () => {
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      clickMoreActions(Account.FIRST, MoreActions.REMOVE_ACCOUNT);
      const account = await browserProxy.whenCalled('removeAccount');
      assertEquals(
          testAccounts[Account.FIRST].principalName, account.principalName);
    });

    test('RemoveAccountShowsToast', async () => {
      const toast = kerberosAccounts.$$('#account-toast');
      assertTrue(!!toast);
      assertFalse(toast.open);

      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      clickMoreActions(Account.FIRST, MoreActions.REMOVE_ACCOUNT);
      await browserProxy.whenCalled('removeAccount');
      await test_util.flushTasks();
      Polymer.dom.flush();
      assertTrue(toast.open);
      assertTrue(kerberosAccounts.$$('#account-toast-label')
                     .innerHTML.includes('removed'));
    });

    test('AccountListIsUpdatedWhenKerberosAccountsUpdates', function() {
      assertEquals(1, browserProxy.getCallCount('getAccounts'));
      cr.webUIListenerCallback('kerberos-accounts-changed');
      assertEquals(2, browserProxy.getCallCount('getAccounts'));
    });

    test('SetAsActiveAccount', async () => {
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      clickMoreActions(Account.SECOND, MoreActions.SET_AS_ACTIVE_ACCOUNT);
      const account = await browserProxy.whenCalled('setAsActiveAccount');
      assertEquals(
          testAccounts[Account.SECOND].principalName, account.principalName);
    });

    test('ShowPolicyIndicatorForManagedAccounts', async () => {
      // Make sure we have at least one managed and one unmanaged account.
      assertFalse(testAccounts[0].isManaged);
      assertTrue(testAccounts[2].isManaged);

      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      accountList =
          kerberosAccounts.shadowRoot.querySelectorAll('.account-list-item');
      assertEquals(testAccounts.length, accountList.length);

      for (let i = 0; i < testAccounts.length; i++) {
        // Assert account has policy indicator iff account is managed.
        const hasAccountPolicyIndicator =
            !!accountList[i].querySelector('.account-policy-indicator');
        assertEquals(testAccounts[i].isManaged, hasAccountPolicyIndicator);

        // Assert 'Remove' button is disabled iff account is managed.
        accountList[i].querySelector('.more-actions').click();
        const moreActions =
            kerberosAccounts.$$('cr-action-menu').querySelectorAll('button');
        const removeAccountButton = moreActions[MoreActions.REMOVE_ACCOUNT];
        assertEquals(testAccounts[i].isManaged, removeAccountButton.disabled);

        // Assert 'Remove' button has policy indicator iff account is managed.
        Polymer.dom.flush();
        const hasRemovalPolicyIndicator = !!removeAccountButton.querySelector(
            '#remove-account-policy-indicator');
        assertEquals(testAccounts[i].isManaged, hasRemovalPolicyIndicator);

        kerberosAccounts.$$('cr-action-menu').close();
      }
    });

    test('AddAccountsAllowed', function() {
      assertTrue(loadTimeData.getBoolean('kerberosAddAccountsAllowed'));
      createDialog();
      assertTrue(!kerberosAccounts.$$('#add-account-policy-indicator'));
      assertFalse(kerberosAccounts.$$('#add-account-button').disabled);
    });

    test('AddAccountsNotAllowed', function() {
      loadTimeData.overrideValues({kerberosAddAccountsAllowed: false});
      createDialog();
      Polymer.dom.flush();
      assertTrue(!!kerberosAccounts.$$('#add-account-policy-indicator'));
      assertTrue(kerberosAccounts.$$('#add-account-button').disabled);

      // Reset for further tests.
      loadTimeData.overrideValues({kerberosAddAccountsAllowed: true});
    });
  });

  // Tests for the kerberos-add-account-dialog element.
  suite('KerberosAddAccountTests', function() {
    let browserProxy = null;

    let dialog = null;
    let addDialog = null;

    let username = null;
    let password = null;
    let rememberPassword = null;
    let advancedConfigButton = null;
    let actionButton = null;
    let generalError = null;
    let title = null;

    // Indices of 'addAccount' params.
    const AddParams = {
      PRINCIPAL_NAME: 0,
      PASSWORD: 1,
      REMEMBER_PASSWORD: 2,
      CONFIG: 3,
      ALLOW_EXISTING: 4,
    };

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      createDialog(null);
    });

    teardown(function() {
      dialog.remove();
    });

    function createDialog(presetAccount) {
      if (dialog) {
        dialog.remove();
      }

      dialog = document.createElement('kerberos-add-account-dialog');
      dialog.presetAccount = presetAccount;
      document.body.appendChild(dialog);

      addDialog = dialog.$.addDialog;
      assertTrue(!!addDialog);

      username = dialog.$.username;
      assertTrue(!!username);

      password = dialog.$.password;
      assertTrue(!!password);

      rememberPassword = dialog.$.rememberPassword;
      assertTrue(!!rememberPassword);

      advancedConfigButton = dialog.$.advancedConfigButton;
      assertTrue(!!advancedConfigButton);

      actionButton = addDialog.querySelector('.action-button');
      assertTrue(!!actionButton);

      generalError = dialog.$['general-error-message'];
      assertTrue(!!generalError);

      title = dialog.$$('[slot=title]').innerText;
      assertTrue(!!title);
    }

    // Sets |error| as error result for addAccount(), simulates a click on the
    // addAccount button and checks that |errorElement| has an non-empty
    // innerText value afterwards.
    async function checkAddAccountError(error, errorElement) {
      Polymer.dom.flush();
      assertEquals(0, errorElement.innerText.length);
      browserProxy.addAccountError = error;
      actionButton.click();
      await browserProxy.whenCalled('addAccount');
      Polymer.dom.flush();
      assertNotEquals(0, errorElement.innerText.length);
    }

    // Opens the Advanced Config dialog, sets |config| as Kerberos configuration
    // and clicks 'Save'. Returns a promise with the validation result.
    function setConfig(config) {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      const configElement = advancedConfigDialog.querySelector('#config');
      assertFalse(configElement.disabled);
      configElement.value = config;
      advancedConfigDialog.querySelector('.action-button').click();
      Polymer.dom.flush();
      return browserProxy.whenCalled('validateConfig');
    }

    // Opens the Advanced Config dialog, asserts that |config| is set as
    // Kerberos configuration and clicks 'Cancel'.
    function assertConfig(config) {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertEquals(config, advancedConfigDialog.querySelector('#config').value);
      advancedConfigDialog.querySelector('.cancel-button').click();
      Polymer.dom.flush();
    }

    // Verifies expected states if no account is preset.
    test('StatesWithoutPresetAccount', function() {
      assertTrue(title.startsWith('Add'));
      assertEquals('Add', actionButton.innerText);
      assertFalse(username.disabled);
      assertEquals('', username.value);
      assertEquals('', password.value);
      assertConfig(loadTimeData.getString('defaultKerberosConfig'));
      assertFalse(rememberPassword.checked);
    });

    // Verifies expected states if an account is preset.
    test('StatesWithPresetAccount', function() {
      createDialog(testAccounts[0]);
      assertTrue(title.startsWith('Refresh'));
      assertEquals('Refresh', actionButton.innerText);
      assertTrue(username.readonly);
      assertEquals(testAccounts[0].principalName, username.value);
      assertConfig(testAccounts[0].config);
      // Password and remember password are tested below since the contents
      // depends on the passwordWasRemembered property of the account.
    });

    // The password input field is empty and 'Remember password' is not preset
    // if |passwordWasRemembered| is false.
    test('PasswordNotPresetIfPasswordWasNotRemembered', function() {
      assertFalse(testAccounts[0].passwordWasRemembered);
      createDialog(testAccounts[0]);
      assertEquals('', password.value);
      assertFalse(rememberPassword.checked);
    });

    // The password input field is not empty and 'Remember password' is preset
    // if |passwordWasRemembered| is true.
    test('PasswordPresetIfPasswordWasRemembered', function() {
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      assertNotEquals('', password.value);
      assertTrue(rememberPassword.checked);
    });

    test('RememberPasswordEnabled', function() {
      assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordEnabled'));
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);

      assertTrue(!dialog.$$('#rememberPasswordPolicyIndicator'));
      assertFalse(rememberPassword.disabled);
      assertTrue(rememberPassword.checked);
      assertNotEquals('', password.value);
    });

    test('RememberPasswordDisabled', function() {
      loadTimeData.overrideValues({kerberosRememberPasswordEnabled: false});
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      Polymer.dom.flush();

      assertTrue(!!dialog.$$('#rememberPasswordPolicyIndicator'));
      assertTrue(rememberPassword.disabled);
      assertFalse(rememberPassword.checked);
      assertEquals('', password.value);

      // Reset for further tests.
      loadTimeData.overrideValues({kerberosRememberPasswordEnabled: true});
    });

    // By clicking the action button, all field values are passed to the
    // 'addAccount' browser proxy method.
    test('ActionButtonPassesFieldValues', async () => {
      const EXPECTED_USER = 'testuser';
      const EXPECTED_PASS = 'testpass';
      const EXPECTED_REMEMBER_PASS = true;
      const EXPECTED_CONFIG = 'testconf';

      username.value = EXPECTED_USER;
      password.value = EXPECTED_PASS;
      const result = await setConfig(EXPECTED_CONFIG);
      rememberPassword.checked = EXPECTED_REMEMBER_PASS;

      assertFalse(actionButton.disabled);
      actionButton.click();
      const args = await browserProxy.whenCalled('addAccount');
      assertEquals(EXPECTED_USER, args[AddParams.PRINCIPAL_NAME]);
      assertEquals(EXPECTED_PASS, args[AddParams.PASSWORD]);
      assertEquals(EXPECTED_REMEMBER_PASS, args[AddParams.REMEMBER_PASSWORD]);
      assertEquals(EXPECTED_CONFIG, args[AddParams.CONFIG]);

      // Should be false if a new account is added. See also
      // AllowExistingIsTrueForPresetAccounts test.
      assertFalse(args[AddParams.ALLOW_EXISTING]);
    });

    // If an account is preset, overwriting that account should be allowed.
    test('AllowExistingIsTrueForPresetAccounts', async () => {
      // Populate dialog with preset account.
      createDialog(testAccounts[1]);
      actionButton.click();
      const args = await browserProxy.whenCalled('addAccount');
      assertTrue(args[AddParams.ALLOW_EXISTING]);
    });

    // While an account is being added, the action button is disabled.
    test('ActionButtonDisableWhileInProgress', async () => {
      assertFalse(actionButton.disabled);
      actionButton.click();
      assertTrue(actionButton.disabled);
      await browserProxy.whenCalled('addAccount');
      assertFalse(actionButton.disabled);
    });

    // If the account has passwordWasRemembered == true and the user just clicks
    // the 'Add' button, an empty password is submitted.
    test('SubmitsEmptyPasswordIfRememberedPasswordIsUsed', async () => {
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      actionButton.click();
      const args = await browserProxy.whenCalled('addAccount');
      assertEquals('', args[AddParams.PASSWORD]);
      assertTrue(args[AddParams.REMEMBER_PASSWORD]);
    });

    // If the account has passwordWasRemembered == true and the user changes the
    // password before clicking the action button, the changed password is
    // submitted.
    test('SubmitsChangedPasswordIfRememberedPasswordIsChanged', async () => {
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      password.inputElement.value = 'some edit';
      password.dispatchEvent(new CustomEvent('input'));
      actionButton.click();
      const args = await browserProxy.whenCalled('addAccount');
      assertNotEquals('', args[AddParams.PASSWORD]);
      assertTrue(args[AddParams.REMEMBER_PASSWORD]);
    });

    test('AdvancedConfigOpenClose', async () => {
      assertTrue(!dialog.$$('#advancedConfigDialog'));
      assertFalse(addDialog.hidden);
      advancedConfigButton.click();
      Polymer.dom.flush();

      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);
      assertTrue(advancedConfigDialog.open);
      assertTrue(addDialog.hidden);
      const saveButton = advancedConfigDialog.querySelector('.action-button');
      assertFalse(saveButton.disabled);
      saveButton.click();
      Polymer.dom.flush();
      assertTrue(saveButton.disabled);

      await browserProxy.whenCalled('validateConfig');
      Polymer.dom.flush();
      assertFalse(saveButton.disabled);
      assertTrue(!dialog.$$('#advancedConfigDialog'));
      assertFalse(addDialog.hidden);
      assertTrue(addDialog.open);
    });

    test('AdvancedConfigurationSaveKeepsConfig', async () => {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);

      // Change config and save.
      const modifiedConfig = 'modified';
      advancedConfigDialog.querySelector('#config').value = modifiedConfig;
      advancedConfigDialog.querySelector('.action-button').click();

      // Changed value should stick.
      await browserProxy.whenCalled('validateConfig');
      Polymer.dom.flush();
      assertConfig(modifiedConfig);
    });

    test('AdvancedConfigurationCancelResetsConfig', function() {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);

      // Change config and cancel.
      const prevConfig = advancedConfigDialog.querySelector('#config').value;
      advancedConfigDialog.querySelector('#config').value = 'modified';
      advancedConfigDialog.querySelector('.cancel-button').click();
      Polymer.dom.flush();

      // Changed value should NOT stick.
      assertConfig(prevConfig);
    });

    test('AdvancedConfigurationDisabledByPolicy', function() {
      assertTrue(testAccounts[2].isManaged);
      createDialog(testAccounts[2]);
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);
      assertTrue(!!advancedConfigDialog.querySelector(
          '#advancedConfigPolicyIndicator'));
      assertTrue(advancedConfigDialog.querySelector('#config').disabled);
    });

    test('AdvancedConfigurationValidationError', async () => {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);

      // Cause a validation error.
      browserProxy.validateConfigResult = {
        error: settings.KerberosErrorType.kBadConfig,
        errorInfo: {
          code: settings.KerberosConfigErrorCode.kKeyNotSupported,
          lineIndex: 0
        }
      };

      // Clicking the action button (aka 'Save') validates the config.
      advancedConfigDialog.querySelector('.action-button').click();

      await browserProxy.whenCalled('validateConfig');

      // Wait for dialog to process the 'validateConfig' result (sets error
      // message etc.).
      await test_util.flushTasks();

      // Is some error text set?
      const configError =
          advancedConfigDialog.querySelector('#config-error-message');
      assertTrue(!!configError);
      assertNotEquals(0, configError.innerText.length);

      // Is something selected?
      const configElement = advancedConfigDialog.querySelector('#config');
      const textArea = configElement.$.input;
      assertEquals(0, textArea.selectionStart);
      assertNotEquals(0, textArea.selectionEnd);

      // Is the config dialog is still open?
      assertTrue(advancedConfigDialog.open);
      assertTrue(addDialog.hidden);

      // Was the config not accepted?
      advancedConfigDialog.querySelector('.cancel-button').click();
      Polymer.dom.flush();
      assertConfig(loadTimeData.getString('defaultKerberosConfig'));
    });

    // addAccount: KerberosErrorType.kNetworkProblem spawns a general error.
    test('AddAccountError_NetworkProblem', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kNetworkProblem, generalError);
    });

    // addAccount: KerberosErrorType.kParsePrincipalFailed spawns a username
    // error.
    test('AddAccountError_ParsePrincipalFailed', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kParsePrincipalFailed, username.$.error);
    });

    // addAccount: KerberosErrorType.kBadPrincipal spawns a username error.
    test('AddAccountError_BadPrincipal', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kBadPrincipal, username.$.error);
    });

    // addAccount: KerberosErrorType.kDuplicatePrincipalName spawns a username
    // error.
    test('AddAccountError_DuplicatePrincipalName', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kDuplicatePrincipalName, username.$.error);
    });

    // addAccount: KerberosErrorType.kContactingKdcFailed spawns a username
    // error.
    test('AddAccountError_ContactingKdcFailed', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kContactingKdcFailed, username.$.error);
    });

    // addAccount: KerberosErrorType.kBadPassword spawns a password error.
    test('AddAccountError_BadPassword', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kBadPassword, password.$.error);
    });

    // addAccount: KerberosErrorType.kPasswordExpired spawns a password error.
    test('AddAccountError_PasswordExpired', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kPasswordExpired, password.$.error);
    });

    // addAccount: KerberosErrorType.kKdcDoesNotSupportEncryptionType spawns a
    // general error.
    test('AddAccountError_KdcDoesNotSupportEncryptionType', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kKdcDoesNotSupportEncryptionType,
          generalError);
    });

    // addAccount: KerberosErrorType.kUnknown spawns a general error.
    test('AddAccountError_Unknown', async () => {
      await checkAddAccountError(
          settings.KerberosErrorType.kUnknown, generalError);
    });
  });
});