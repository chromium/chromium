// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import 'chrome://os-settings/lazy_load.js';

import {KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType} from 'chrome://os-settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {AccountIndex, TestKerberosAccountsBrowserProxy, TEST_KERBEROS_ACCOUNTS} from './test_kerberos_accounts_browser_proxy.js';

// Tests for the kerberos-add-account-dialog element.
suite('Kerberos add account dialog tests', function() {
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

    // Setting the default value of the relevant load time data.
    loadTimeData.overrideValues({kerberosRememberPasswordByDefault: true});
    loadTimeData.overrideValues({kerberosRememberPasswordEnabled: true});
    loadTimeData.overrideValues({isGuest: false});
    loadTimeData.overrideValues({kerberosDomainAutocomplete: ''});

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
    flush();

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

  // Verifies expected states if no account is preset and password should be
  // remembered by default.
  test('State without preset account and remember password check', async () => {
    assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordByDefault'));

    assertTrue(title.startsWith('Add'));
    assertEquals('Add', actionButton.innerText);
    assertFalse(username.disabled);
    assertEquals('', username.value);
    assertEquals('', password.value);
    assertConfig(loadTimeData.getString('defaultKerberosConfig'));
    assertTrue(rememberPassword.checked);
  });

  // Verifies the rememberPassword state if no account is preset and password
  // should not be remembered by default.
  test('State without preset account and password not remembered', async () => {
    loadTimeData.overrideValues({kerberosRememberPasswordByDefault: false});
    createDialog(null);

    assertFalse(rememberPassword.checked);
  });

  // Verifies expected state if an account is preset.
  test('State with preset account', async () => {
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST]);
    assertTrue(title.startsWith('Refresh'));
    assertEquals('Refresh', actionButton.innerText);
    assertTrue(username.readonly);
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].principalName,
        username.value);
    assertConfig(TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].config);
    // Password and remember password are tested below since the contents
    // depends on the passwordWasRemembered property of the account.
  });

  // The password input field is empty and 'Remember password' is checked if
  // |passwordWasRemembered| is false and the password should be remembered by
  // default.
  test('Password not preset if password was not remembered', function() {
    assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordByDefault'));
    assertFalse(TEST_KERBEROS_ACCOUNTS[0].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[0]);

    assertEquals('', password.value);
    assertTrue(rememberPassword.checked);
  });

  // The password input field is empty and 'Remember password' is not checked if
  // |passwordWasRemembered| is false and the password should not be remembered
  // by default.
  test(
      'Checkbox unchecked if password was not remembered and feature disabled',
      function() {
        loadTimeData.overrideValues({kerberosRememberPasswordByDefault: false});
        createDialog(null);

        assertFalse(
            TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].passwordWasRemembered);
        createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST]);
        assertEquals('', password.value);
        assertFalse(rememberPassword.checked);
      });

  // The password input field is not empty and 'Remember password' is checked
  // if |passwordWasRemembered| is true.
  test('Password preset if password was remembered', function() {
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND]);
    assertNotEquals('', password.value);
    assertTrue(rememberPassword.checked);
  });

  test('Remember password enabled', function() {
    assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordEnabled'));
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND]);

    assertTrue(
        !dialog.shadowRoot.querySelector('#rememberPasswordPolicyIndicator'));
    assertFalse(rememberPassword.disabled);
    assertTrue(rememberPassword.checked);
    assertNotEquals('', password.value);
  });

  test('Remember password disabled', function() {
    loadTimeData.overrideValues({kerberosRememberPasswordEnabled: false});
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND]);

    assertTrue(
        !!dialog.shadowRoot.querySelector('#rememberPasswordPolicyIndicator'));
    assertTrue(rememberPassword.disabled);
    assertFalse(rememberPassword.checked);
    assertEquals('', password.value);
  });

  test('Remember password visible and checked on user sessions', function() {
    assertFalse(loadTimeData.getBoolean('isGuest'));
    createDialog(null);

    assertFalse(
        dialog.shadowRoot.querySelector('#rememberPasswordContainer').hidden);
    assertTrue(rememberPassword.checked);
  });

  test('Remember password hidden and not checked on MGS', function() {
    loadTimeData.overrideValues({isGuest: true});
    createDialog(null);

    assertTrue(
        dialog.shadowRoot.querySelector('#rememberPasswordContainer').hidden);
    assertFalse(rememberPassword.checked);
  });

  // By clicking the action button, all field values are passed to the
  // 'addAccount' browser proxy method.
  test('Action button passes field values', async () => {
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
  test('Allow existing is true for preset accounts', async () => {
    // Populate dialog with preset account.
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND]);
    actionButton.click();
    const args = await browserProxy.whenCalled('addAccount');
    assertTrue(args[AddParams.ALLOW_EXISTING]);
  });

  // While an account is being added, the action button is disabled.
  test('Action button disable while in progress', async () => {
    assertFalse(actionButton.disabled);
    actionButton.click();
    assertTrue(actionButton.disabled);
    await browserProxy.whenCalled('addAccount');
    assertFalse(actionButton.disabled);
  });

  // If the account has passwordWasRemembered === true and the user just
  // clicks the 'Add' button, an empty password is submitted.
  test('Submits empty password if remembered password is used', async () => {
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND]);
    actionButton.click();
    const args = await browserProxy.whenCalled('addAccount');
    assertEquals('', args[AddParams.PASSWORD]);
    assertTrue(args[AddParams.REMEMBER_PASSWORD]);
  });

  // If the account has passwordWasRemembered === true and the user changes
  // the password before clicking the action button, the changed password is
  // submitted.
  test('Submits changed password if password field was changed', async () => {
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].passwordWasRemembered);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND]);
    password.inputElement.value = 'some edit';
    password.dispatchEvent(new CustomEvent('input'));
    actionButton.click();
    const args = await browserProxy.whenCalled('addAccount');
    assertNotEquals('', args[AddParams.PASSWORD]);
    assertTrue(args[AddParams.REMEMBER_PASSWORD]);
  });

  test('Advanced config open and close', async () => {
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

  test('Advanced configuration save keeps config', async () => {
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

  test('Advanced configuration cancel resets config', async () => {
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

  test('Advanced configuration disabled by policy', async () => {
    assertTrue(TEST_KERBEROS_ACCOUNTS[AccountIndex.THIRD].isManaged);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.THIRD]);
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

  test('Advanced configuration validation error', async () => {
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

  test('Validate configuration on advanced click', async () => {
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

  test('Domain autocomplete enabled', function() {
    loadTimeData.overrideValues({kerberosDomainAutocomplete: 'domain.com'});
    createDialog(null);

    // '@' should be automatically added to the policy value.
    assertEquals(
        '@domain.com',
        dialog.shadowRoot.querySelector('#kerberosDomain').innerText);
  });

  test('Domain autocomplete enabled override', function() {
    loadTimeData.overrideValues({kerberosDomainAutocomplete: 'domain.com'});
    assertTrue(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].principalName &&
        TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].principalName.indexOf(
            '@') !== -1);
    createDialog(TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST]);

    // If inserted principal contains '@', nothing should be shown.
    assertEquals(
        '', dialog.shadowRoot.querySelector('#kerberosDomain').innerText);
  });

  test('Domain autocomplete disabled', function() {
    assertEquals('', loadTimeData.getString('kerberosDomainAutocomplete'));
    assertEquals(
        '', dialog.shadowRoot.querySelector('#kerberosDomain').innerText);
  });

  // Testing how `addAccount` error types are handled by the UI:

  // KerberosErrorType.PARSE_PRINCIPAL_FAILED spawns a username error.
  test('Add account error - parse principal failed', async () => {
    await checkAddAccountError(
        KerberosErrorType.PARSE_PRINCIPAL_FAILED, username.$.error);
  });

  // KerberosErrorType.BAD_PRINCIPAL spawns a username error.
  test('Add account error - bad principal', async () => {
    await checkAddAccountError(
        KerberosErrorType.BAD_PRINCIPAL, username.$.error);
  });

  // KerberosErrorType.DUPLICATE_PRINCIPAL_NAME spawns a username error.
  test('Add account error - duplicate principal name', async () => {
    await checkAddAccountError(
        KerberosErrorType.DUPLICATE_PRINCIPAL_NAME, username.$.error);
  });

  // KerberosErrorType.CONTACTING_KDC_FAILED spawns a username error.
  test('Add account error - contacting KDC failed', async () => {
    await checkAddAccountError(
        KerberosErrorType.CONTACTING_KDC_FAILED, username.$.error);
  });

  // KerberosErrorType.BAD_PASSWORD spawns a password error.
  test('Add account error - bad password', async () => {
    await checkAddAccountError(
        KerberosErrorType.BAD_PASSWORD, password.$.error);
  });

  // KerberosErrorType.PASSWORD_EXPIRED spawns a password error.
  test('Add account error - password expired', async () => {
    await checkAddAccountError(
        KerberosErrorType.PASSWORD_EXPIRED, password.$.error);
  });

  // KerberosErrorType.NETWORK_PROBLEM spawns a general error.
  test('Add account error - network problem', async () => {
    await checkAddAccountError(KerberosErrorType.NETWORK_PROBLEM, generalError);
  });

  // KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE spawns a general
  // error.
  test('Add account error - KDC does not support encryption type', async () => {
    await checkAddAccountError(
        KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE, generalError);
  });

  // KerberosErrorType.UNKNOWN spawns a general error.
  test('Add account error - unknown', async () => {
    await checkAddAccountError(KerberosErrorType.UNKNOWN, generalError);
  });

  // KerberosErrorType.BAD_CONFIG spawns a general error.
  test('Add account error - bad config', async () => {
    await checkAddAccountError(KerberosErrorType.BAD_CONFIG, generalError);
  });
});
