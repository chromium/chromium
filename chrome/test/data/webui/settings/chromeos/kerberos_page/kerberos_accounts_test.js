// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {TestKerberosAccountsBrowserProxy, TEST_KERBEROS_ACCOUNTS} from './test_kerberos_accounts_browser_proxy.js';
import {Router, Route, routes, KerberosErrorType, KerberosConfigErrorCode, KerberosAccountsBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';

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
    routes.BASIC = new Route('/'),
    routes.KERBEROS = routes.BASIC.createSection('/kerberos', 'kerberos');
    routes.KERBEROS_ACCOUNTS_V2 =
        routes.KERBEROS.createChild('/kerberos/kerberosAccounts');

    Router.resetInstanceForTesting(new Router(routes));

    browserProxy = new TestKerberosAccountsBrowserProxy();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    createDialog();
  });

  teardown(function() {
    kerberosAccounts.remove();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(undefined);
  });

  function createDialog() {
    if (kerberosAccounts) {
      kerberosAccounts.remove();
    }

    kerberosAccounts = document.createElement('settings-kerberos-accounts');
    document.body.appendChild(kerberosAccounts);

    accountList = kerberosAccounts.shadowRoot.querySelector('#account-list');
    assertTrue(!!accountList);
  }

  function clickMoreActions(accountIndex, moreActionsIndex) {
    // Click 'More actions' for the given account.
    kerberosAccounts.shadowRoot.querySelectorAll('.more-actions')[accountIndex]
        .click();
    // Click on the given action.
    kerberosAccounts.shadowRoot.querySelector('cr-action-menu')
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
    flush();
    // The test accounts were added in |getAccounts()| mock above.
    assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountList.items.length);
  });

  test('AccountListSignedInSignedOutLabels', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
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
    // The kerberos-add-account-dialog shouldn't be open yet.
    assertTrue(!kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog'));

    kerberosAccounts.shadowRoot.querySelector('#add-account-button').click();
    flush();

    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals('', addDialog.$.username.value);
  });

  test('ReauthenticateAccount', async () => {
    // Wait until accounts are loaded.
    await browserProxy.whenCalled('getAccounts');
    flush();

    // The kerberos-add-account-dialog shouldn't be open yet.
    assertTrue(!kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog'));

    // Click "Sign-In" on an existing account.
    // Note that both accounts have a reauth button, but the first one is
    // hidden, so click the second one (clicking a hidden button works, but
    // it feels weird).
    kerberosAccounts.shadowRoot
        .querySelectorAll('.reauth-button')[Account.SECOND]
        .click();
    flush();

    // Now the kerberos-add-account-dialog should be open with preset
    // username.
    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[Account.SECOND].principalName,
        addDialog.$.username.value);
  });

  // Appending '?kerberos_reauth=<principal>' to the URL opens the reauth
  // dialog for that account.
  test('HandleReauthQueryParameter', async () => {
    const principal_name = TEST_KERBEROS_ACCOUNTS[Account.FIRST].principalName;
    const params = new URLSearchParams();
    params.append('kerberos_reauth', principal_name);
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2, params);

    // The flushTasks is necessary since the kerberos_reauth param would
    // otherwise be handled AFTER the callback below is executed.
    await browserProxy.whenCalled('getAccounts');
    await flushTasks();
    flush();
    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals(principal_name, addDialog.$.username.value);
  });

  test('RefreshNow', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(Account.FIRST, MoreActions.REFRESH_NOW);
    flush();

    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[Account.FIRST].principalName,
        addDialog.$.username.value);
  });

  test('RefreshAccountShowsToast', async () => {
    const toast = kerberosAccounts.shadowRoot.querySelector('#account-toast');
    assertTrue(!!toast);
    assertFalse(toast.open);

    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(Account.FIRST, MoreActions.REFRESH_NOW);
    flush();

    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    addDialog.shadowRoot.querySelector('.action-button').click();
    flush();

    await onEvent(addDialog, 'close');
    await flushTasks();
    flush();
    assertTrue(toast.open);
    assertTrue(kerberosAccounts.shadowRoot.querySelector('#account-toast-label')
                   .innerHTML.includes('refreshed'));
  });

  test('RemoveAccount', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(Account.FIRST, MoreActions.REMOVE_ACCOUNT);
    const account = await browserProxy.whenCalled('removeAccount');
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[Account.FIRST].principalName,
        account.principalName);
  });

  test('Deep link to remove account dropdown', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1801');
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2, params);

    await browserProxy.whenCalled('getAccounts');
    await flushTasks();
    flush();

    const deepLinkElement =
        kerberosAccounts.root.querySelectorAll('cr-icon-button')[0];
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Kebab menu should be focused for settingId=1801.');
  });

  test('RemoveAccountShowsToast', async () => {
    const toast = kerberosAccounts.shadowRoot.querySelector('#account-toast');
    assertTrue(!!toast);
    assertFalse(toast.open);

    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(Account.FIRST, MoreActions.REMOVE_ACCOUNT);
    await browserProxy.whenCalled('removeAccount');
    await flushTasks();
    flush();
    assertTrue(toast.open);
    assertTrue(kerberosAccounts.shadowRoot.querySelector('#account-toast-label')
                   .innerHTML.includes('removed'));
  });

  test('AccountListIsUpdatedWhenKerberosAccountsUpdates', function() {
    assertEquals(1, browserProxy.getCallCount('getAccounts'));
    webUIListenerCallback('kerberos-accounts-changed');
    assertEquals(2, browserProxy.getCallCount('getAccounts'));
  });

  test('SetAsActiveAccount', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
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
    flush();
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
          kerberosAccounts.shadowRoot.querySelector('cr-action-menu')
              .querySelectorAll('button');
      const removeAccountButton = moreActions[MoreActions.REMOVE_ACCOUNT];
      assertEquals(
          TEST_KERBEROS_ACCOUNTS[i].isManaged, removeAccountButton.disabled);

      // Assert 'Remove' button has policy indicator iff account is managed.
      flush();
      const hasRemovalPolicyIndicator = !!removeAccountButton.querySelector(
          '#remove-account-policy-indicator');
      assertEquals(
          TEST_KERBEROS_ACCOUNTS[i].isManaged, hasRemovalPolicyIndicator);

      kerberosAccounts.shadowRoot.querySelector('cr-action-menu').close();
    }
  });

  test('AddAccountsAllowed', function() {
    assertTrue(loadTimeData.getBoolean('kerberosAddAccountsAllowed'));
    createDialog();
    assertTrue(!kerberosAccounts.shadowRoot.querySelector(
        '#add-account-policy-indicator'));
    assertFalse(kerberosAccounts.shadowRoot.querySelector('#add-account-button')
                    .disabled);
  });

  test('AddAccountsNotAllowed', function() {
    loadTimeData.overrideValues({kerberosAddAccountsAllowed: false});
    createDialog();
    flush();
    assertTrue(!!kerberosAccounts.shadowRoot.querySelector(
        '#add-account-policy-indicator'));
    assertTrue(kerberosAccounts.shadowRoot.querySelector('#add-account-button')
                   .disabled);

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
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    createDialog(null);
  });

  teardown(function() {
    dialog.remove();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(undefined);
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

    title = dialog.shadowRoot.querySelector('[slot=title]').innerText;
    assertTrue(!!title);
  }

  // Sets |error| as error result for addAccount(), simulates a click on the
  // addAccount button and checks that |errorElement| has an non-empty
  // innerText value afterwards.
  async function checkAddAccountError(error, errorElement) {
    flush();
    assertEquals(0, errorElement.innerText.length);
    browserProxy.addAccountError = error;
    actionButton.click();
    await browserProxy.whenCalled('addAccount');
    flush();
    assertNotEquals(0, errorElement.innerText.length);
  }

  // Opens the Advanced Config dialog, sets |config| as Kerberos configuration
  // and clicks 'Save'. Returns a promise with the validation result.
  async function setConfig(config) {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    const configElement = advancedConfigDialog.querySelector('#config');
    assertFalse(configElement.disabled);
    configElement.value = config;
    advancedConfigDialog.querySelector('.action-button').click();
    flush();
    return browserProxy.whenCalled('validateConfig');
  }

  // Opens the Advanced Config dialog, asserts that |config| is set as
  // Kerberos configuration and clicks 'Cancel'.
  async function assertConfig(config) {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertEquals(config, advancedConfigDialog.querySelector('#config').value);
    advancedConfigDialog.querySelector('.cancel-button').click();
    flush();
  }

  // Verifies expected states if no account is preset.
  test('StatesWithoutPresetAccount', async () => {
    assertTrue(title.startsWith('Add'));
    assertEquals('Add', actionButton.innerText);
    assertFalse(username.disabled);
    assertEquals('', username.value);
    assertEquals('', password.value);
    assertConfig(loadTimeData.getString('defaultKerberosConfig'));
    assertFalse(rememberPassword.checked);
  });

  // Verifies expected states if an account is preset.
  test('StatesWithPresetAccount', async () => {
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

    assertTrue(
        !dialog.shadowRoot.querySelector('#rememberPasswordPolicyIndicator'));
    assertFalse(rememberPassword.disabled);
    assertTrue(rememberPassword.checked);
    assertNotEquals('', password.value);
  });

  test('RememberPasswordDisabled', function() {
    loadTimeData.overrideValues({kerberosRememberPasswordEnabled: false});
    assertTrue(TEST_KERBEROS_ACCOUNTS[1].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[1]);
    flush();

    assertTrue(
        !!dialog.shadowRoot.querySelector('#rememberPasswordPolicyIndicator'));
    assertTrue(rememberPassword.disabled);
    assertFalse(rememberPassword.checked);
    assertEquals('', password.value);

    // Reset for further tests.
    loadTimeData.overrideValues({kerberosRememberPasswordEnabled: true});
  });

  test('RememberPasswordVisibleOnUserSessions', function() {
    assertFalse(loadTimeData.getBoolean('isGuest'));
    createDialog(null);
    flush();

    assertFalse(
        dialog.shadowRoot.querySelector('#rememberPasswordContainer').hidden);
  });

  test('RememberPasswordHiddenOnMgs', function() {
    loadTimeData.overrideValues({isGuest: true});
    createDialog(null);
    flush();

    assertTrue(
        dialog.shadowRoot.querySelector('#rememberPasswordContainer').hidden);

    // Reset for further tests.
    loadTimeData.overrideValues({isGuest: false});
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
    assertTrue(!dialog.shadowRoot.querySelector('#advancedConfigDialog'));
    assertFalse(addDialog.hidden);
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();

    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);
    assertTrue(advancedConfigDialog.open);
    assertTrue(addDialog.hidden);
    const saveButton = advancedConfigDialog.querySelector('.action-button');
    assertFalse(saveButton.disabled);
    saveButton.click();
    flush();
    assertTrue(saveButton.disabled);

    await browserProxy.whenCalled('validateConfig');
    flush();
    assertFalse(saveButton.disabled);
    assertTrue(!dialog.shadowRoot.querySelector('#advancedConfigDialog'));
    assertFalse(addDialog.hidden);
    assertTrue(addDialog.open);
  });

  test('AdvancedConfigurationSaveKeepsConfig', async () => {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Change config and save.
    const modifiedConfig = 'modified';
    advancedConfigDialog.querySelector('#config').value = modifiedConfig;
    advancedConfigDialog.querySelector('.action-button').click();

    // Changed value should stick.
    await browserProxy.whenCalled('validateConfig');
    flush();
    assertConfig(modifiedConfig);
  });

  test('AdvancedConfigurationCancelResetsConfig', async () => {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Change config and cancel.
    const prevConfig = advancedConfigDialog.querySelector('#config').value;
    advancedConfigDialog.querySelector('#config').value = 'modified';
    advancedConfigDialog.querySelector('.cancel-button').click();
    flush();

    // Changed value should NOT stick.
    assertConfig(prevConfig);
  });

  test('AdvancedConfigurationDisabledByPolicy', async () => {
    assertTrue(TEST_KERBEROS_ACCOUNTS[2].isManaged);
    createDialog(TEST_KERBEROS_ACCOUNTS[2]);
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);
    assertTrue(
        !!advancedConfigDialog.querySelector('#advancedConfigPolicyIndicator'));
    assertTrue(advancedConfigDialog.querySelector('#config').disabled);
  });

  test('AdvancedConfigurationValidationError', async () => {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Cause a validation error.
    browserProxy.validateConfigResult = {
      error: KerberosErrorType.BAD_CONFIG,
      errorInfo:
          {code: KerberosConfigErrorCode.KEY_NOT_SUPPORTED, lineIndex: 0},
    };

    // Clicking the action button (aka 'Save') validates the config.
    advancedConfigDialog.querySelector('.action-button').click();

    await browserProxy.whenCalled('validateConfig');

    // Wait for dialog to process the 'validateConfig' result (sets error
    // message etc.).
    await flushTasks();

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

    // Is the config dialog still open?
    assertTrue(advancedConfigDialog.open);
    assertTrue(addDialog.hidden);

    // Was the config not accepted?
    advancedConfigDialog.querySelector('.cancel-button').click();
    flush();
    assertConfig(loadTimeData.getString('defaultKerberosConfig'));
  });

  test('ValidateConfigurationOnAdvancedClick', async () => {
    // Cause a validation error.
    browserProxy.validateConfigResult = {
      error: KerberosErrorType.BAD_CONFIG,
      errorInfo:
          {code: KerberosConfigErrorCode.KEY_NOT_SUPPORTED, lineIndex: 0},
    };

    // Validating happens on "Advanced" click.
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');

    // Wait for dialog to process the 'validateConfig' result (sets error
    // message etc.).
    await flushTasks();

    const advancedConfigDialog =
        dialog.shadowRoot.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

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
  });

  test('DomainAutocompleteEnabled', function() {
    loadTimeData.overrideValues({kerberosDomainAutocomplete: 'domain.com'});
    createDialog();
    flush();

    // '@' should be automatically added to the policy value.
    assertEquals(
        '@domain.com',
        dialog.shadowRoot.querySelector('#kerberosDomain').innerText);

    // Reset for further tests.
    loadTimeData.overrideValues({kerberosDomainAutocomplete: ''});
  });

  test('DomainAutocompleteEnabledOverride', function() {
    loadTimeData.overrideValues({kerberosDomainAutocomplete: 'domain.com'});
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[0].principalName &&
        TEST_KERBEROS_ACCOUNTS[0].principalName.indexOf('@') !== -1);
    createDialog(TEST_KERBEROS_ACCOUNTS[0]);
    flush();

    // If inserted principal contains '@', nothing should be shown.
    assertEquals(
        '', dialog.shadowRoot.querySelector('#kerberosDomain').innerText);

    // Reset for further tests.
    loadTimeData.overrideValues({kerberosDomainAutocomplete: ''});
  });

  test('DomainAutocompleteDisabled', function() {
    assertEquals('', loadTimeData.getString('kerberosDomainAutocomplete'));
    assertEquals(
        '', dialog.shadowRoot.querySelector('#kerberosDomain').innerText);
  });

  // addAccount: KerberosErrorType.kNetworkProblem spawns a general error.
  test('AddAccountError_NetworkProblem', async () => {
    await checkAddAccountError(KerberosErrorType.NETWORK_PROBLEM, generalError);
  });

  // addAccount: KerberosErrorType.kParsePrincipalFailed spawns a username
  // error.
  test('AddAccountError_ParsePrincipalFailed', async () => {
    await checkAddAccountError(
        KerberosErrorType.PARSE_PRINCIPAL_FAILED, username.$.error);
  });

  // addAccount: KerberosErrorType.BAD_PRINCIPAL spawns a username error.
  test('AddAccountError_BadPrincipal', async () => {
    await checkAddAccountError(
        KerberosErrorType.BAD_PRINCIPAL, username.$.error);
  });

  // addAccount: KerberosErrorType.DUPLICATE_PRINCIPAL_NAME spawns a username
  // error.
  test('AddAccountError_DuplicatePrincipalName', async () => {
    await checkAddAccountError(
        KerberosErrorType.DUPLICATE_PRINCIPAL_NAME, username.$.error);
  });

  // addAccount: KerberosErrorType.CONTACTING_KDC_FAILED spawns a username
  // error.
  test('AddAccountError_ContactingKdcFailed', async () => {
    await checkAddAccountError(
        KerberosErrorType.CONTACTING_KDC_FAILED, username.$.error);
  });

  // addAccount: KerberosErrorType.BAD_PASSWORD spawns a password error.
  test('AddAccountError_BadPassword', async () => {
    await checkAddAccountError(
        KerberosErrorType.BAD_PASSWORD, password.$.error);
  });

  // addAccount: KerberosErrorType.PASSWORD_EXPIRED spawns a password error.
  test('AddAccountError_PasswordExpired', async () => {
    await checkAddAccountError(
        KerberosErrorType.PASSWORD_EXPIRED, password.$.error);
  });

  // addAccount: KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE spawns
  // a general error.
  test('AddAccountError_KdcDoesNotSupportEncryptionType', async () => {
    await checkAddAccountError(
        KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE, generalError);
  });

  // addAccount: KerberosErrorType.UNKNOWN spawns a general error.
  test('AddAccountError_Unknown', async () => {
    await checkAddAccountError(KerberosErrorType.UNKNOWN, generalError);
  });
});
