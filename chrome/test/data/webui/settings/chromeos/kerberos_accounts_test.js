// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestKerberosAccountsBrowserProxy, TEST_KERBEROS_ACCOUNTS} from './test_kerberos_accounts_browser_proxy.m.js';
// #import {Router, Route, routes, KerberosErrorType, KerberosConfigErrorCode, KerberosAccountsBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

cr.define('settings_kerberos_accounts', function() {
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
      settings.routes.BASIC = new settings.Route('/'),
      settings.routes.KERBEROS =
          settings.routes.BASIC.createSection('/kerberos', 'kerberos');
      settings.routes.KERBEROS_ACCOUNTS_V2 =
          settings.routes.KERBEROS.createChild('/kerberos/kerberosAccounts');

      settings.Router.resetInstanceForTesting(
          new settings.Router(settings.routes));

      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      createDialog();
    });

    teardown(function() {
      kerberosAccounts.remove();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = undefined;
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
      assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountList.items.length);
    });

    test('AccountListSignedInSignedOutLabels', async () => {
      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      accountList =
          kerberosAccounts.shadowRoot.querySelectorAll('.account-list-item');
      assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountList.length);

      // Show 'Valid for <duration>' for accounts that are signed in.
      let signedIn = accountList[0].querySelector('.signed-in');
      let signedOut = accountList[0].querySelector('.signed-out');
      assertTrue(TEST_KERBEROS_ACCOUNTS[0].isSignedIn);
      assertFalse(signedIn.hidden);
      assertTrue(signedOut.hidden);
      assertEquals(
          'Valid for ' + TEST_KERBEROS_ACCOUNTS[0].validForDuration,
          signedIn.innerText);

      // Show 'Expired' for accounts that are not signed in.
      signedIn = accountList[1].querySelector('.signed-in');
      signedOut = accountList[1].querySelector('.signed-out');
      assertFalse(TEST_KERBEROS_ACCOUNTS[1].isSignedIn);
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
          TEST_KERBEROS_ACCOUNTS[Account.SECOND].principalName,
          addDialog.$.username.value);
    });

    // Appending '?kerberos_reauth=<principal>' to the URL opens the reauth
    // dialog for that account.
    test('HandleReauthQueryParameter', async () => {
      const principal_name =
          TEST_KERBEROS_ACCOUNTS[Account.FIRST].principalName;
      const params = new URLSearchParams;
      params.append('kerberos_reauth', principal_name);
      settings.Router.getInstance().navigateTo(
          settings.routes.KERBEROS_ACCOUNTS_V2, params);

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
          TEST_KERBEROS_ACCOUNTS[Account.FIRST].principalName,
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
          TEST_KERBEROS_ACCOUNTS[Account.FIRST].principalName,
          account.principalName);
    });

    test('Deep link to remove account dropdown', async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});

      const params = new URLSearchParams;
      params.append('settingId', '1801');
      settings.Router.getInstance().navigateTo(
          settings.routes.KERBEROS_ACCOUNTS_V2, params);

      await browserProxy.whenCalled('getAccounts');
      await test_util.flushTasks();
      Polymer.dom.flush();

      const deepLinkElement =
          kerberosAccounts.root.querySelectorAll('cr-icon-button')[0];
      assertTrue(!!deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Kebab menu should be focused for settingId=1801.');
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
          TEST_KERBEROS_ACCOUNTS[Account.SECOND].principalName,
          account.principalName);
    });

    test('ShowPolicyIndicatorForManagedAccounts', async () => {
      // Make sure we have at least one managed and one unmanaged account.
      assertFalse(TEST_KERBEROS_ACCOUNTS[0].isManaged);
      assertTrue(TEST_KERBEROS_ACCOUNTS[2].isManaged);

      await browserProxy.whenCalled('getAccounts');
      Polymer.dom.flush();
      accountList =
          kerberosAccounts.shadowRoot.querySelectorAll('.account-list-item');
      assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountList.length);

      for (let i = 0; i < TEST_KERBEROS_ACCOUNTS.length; i++) {
        // Assert account has policy indicator iff account is managed.
        const hasAccountPolicyIndicator =
            !!accountList[i].querySelector('.account-policy-indicator');
        assertEquals(
            TEST_KERBEROS_ACCOUNTS[i].isManaged, hasAccountPolicyIndicator);

        // Assert 'Remove' button is disabled iff account is managed.
        accountList[i].querySelector('.more-actions').click();
        const moreActions =
            kerberosAccounts.$$('cr-action-menu').querySelectorAll('button');
        const removeAccountButton = moreActions[MoreActions.REMOVE_ACCOUNT];
        assertEquals(
            TEST_KERBEROS_ACCOUNTS[i].isManaged, removeAccountButton.disabled);

        // Assert 'Remove' button has policy indicator iff account is managed.
        Polymer.dom.flush();
        const hasRemovalPolicyIndicator = !!removeAccountButton.querySelector(
            '#remove-account-policy-indicator');
        assertEquals(
            TEST_KERBEROS_ACCOUNTS[i].isManaged, hasRemovalPolicyIndicator);

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
      settings.KerberosAccountsBrowserProxyImpl.instance_ = undefined;
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
      createDialog(TEST_KERBEROS_ACCOUNTS[0]);
      assertTrue(title.startsWith('Refresh'));
      assertEquals('Refresh', actionButton.innerText);
      assertTrue(username.readonly);
      assertEquals(TEST_KERBEROS_ACCOUNTS[0].principalName, username.value);
      assertConfig(TEST_KERBEROS_ACCOUNTS[0].config);
      // Password and remember password are tested below since the contents
      // depends on the passwordWasRemembered property of the account.
    });

    // The password input field is empty and 'Remember password' is not preset
    // if |passwordWasRemembered| is false.
    test('PasswordNotPresetIfPasswordWasNotRemembered', function() {
      assertFalse(TEST_KERBEROS_ACCOUNTS[0].passwordWasRemembered);
      createDialog(TEST_KERBEROS_ACCOUNTS[0]);
      assertEquals('', password.value);
      assertFalse(rememberPassword.checked);
    });

    // The password input field is not empty and 'Remember password' is preset
    // if |passwordWasRemembered| is true.
    test('PasswordPresetIfPasswordWasRemembered', function() {
      assertTrue(TEST_KERBEROS_ACCOUNTS[1].passwordWasRemembered);
      createDialog(TEST_KERBEROS_ACCOUNTS[1]);
      assertNotEquals('', password.value);
      assertTrue(rememberPassword.checked);
    });

    test('RememberPasswordEnabled', function() {
      assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordEnabled'));
      assertTrue(TEST_KERBEROS_ACCOUNTS[1].passwordWasRemembered);
      createDialog(TEST_KERBEROS_ACCOUNTS[1]);

      assertTrue(!dialog.$$('#rememberPasswordPolicyIndicator'));
      assertFalse(rememberPassword.disabled);
      assertTrue(rememberPassword.checked);
      assertNotEquals('', password.value);
    });

    test('RememberPasswordDisabled', function() {
      loadTimeData.overrideValues({kerberosRememberPasswordEnabled: false});
      assertTrue(TEST_KERBEROS_ACCOUNTS[1].passwordWasRemembered);
      createDialog(TEST_KERBEROS_ACCOUNTS[1]);
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
      createDialog(TEST_KERBEROS_ACCOUNTS[1]);
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

    // If the account has passwordWasRemembered === true and the user just
    // clicks the 'Add' button, an empty password is submitted.
    test('SubmitsEmptyPasswordIfRememberedPasswordIsUsed', async () => {
      assertTrue(TEST_KERBEROS_ACCOUNTS[1].passwordWasRemembered);
      createDialog(TEST_KERBEROS_ACCOUNTS[1]);
      actionButton.click();
      const args = await browserProxy.whenCalled('addAccount');
      assertEquals('', args[AddParams.PASSWORD]);
      assertTrue(args[AddParams.REMEMBER_PASSWORD]);
    });

    // If the account has passwordWasRemembered === true and the user changes
    // the password before clicking the action button, the changed password is
    // submitted.
    test('SubmitsChangedPasswordIfRememberedPasswordIsChanged', async () => {
      assertTrue(TEST_KERBEROS_ACCOUNTS[1].passwordWasRemembered);
      createDialog(TEST_KERBEROS_ACCOUNTS[1]);
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
      assertTrue(TEST_KERBEROS_ACCOUNTS[2].isManaged);
      createDialog(TEST_KERBEROS_ACCOUNTS[2]);
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

  // #cr_define_end
  return {};
});
