// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {KerberosAccount, KerberosAccountsBrowserProxyImpl, KerberosAddAccountDialogElement, KerberosConfigErrorCode, KerberosErrorType} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrCheckboxElement, CrDialogElement, CrInputElement, CrTextareaElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {AccountIndex, TEST_KERBEROS_ACCOUNTS, TestKerberosAccountsBrowserProxy} from './test_kerberos_accounts_browser_proxy.js';

suite('<kerberos-add-account-dialog>', () => {
  let browserProxy: TestKerberosAccountsBrowserProxy;
  let dialog: KerberosAddAccountDialogElement;

  let addDialog: CrDialogElement;
  let username: CrInputElement;
  let password: CrInputElement;

  let rememberPassword: CrCheckboxElement;
  let advancedConfigButton: HTMLElement;
  let actionButton: CrButtonElement;
  let generalError: HTMLElement;
  let title: string;

  // Indices of 'addAccount' params.
  const enum AddParams {
    PRINCIPAL_NAME = 0,
    PASSWORD = 1,
    REMEMBER_PASSWORD = 2,
    CONFIG = 3,
    ALLOW_EXISTING = 4,
  }

  setup(() => {
    browserProxy = new TestKerberosAccountsBrowserProxy();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(browserProxy);

    loadTimeData.overrideValues({
      kerberosRememberPasswordByDefault: true,
      kerberosRememberPasswordEnabled: true,
      isGuest: false,
      kerberosDomainAutocomplete: '',
    });

    createDialog(null);
  });

  teardown(() => {
    dialog.remove();
    browserProxy.reset();
  });

  function createDialog(presetAccount: KerberosAccount|null): void {
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

    // CrCheckboxElement
    const checkbox =
        addDialog.querySelector<CrCheckboxElement>('#rememberPassword');
    assertTrue(!!checkbox);
    rememberPassword = checkbox;

    const configButton =
        addDialog.querySelector<HTMLElement>('#advancedConfigButton');
    assertTrue(!!configButton);
    advancedConfigButton = configButton;

    const button = addDialog.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!button);
    actionButton = button;

    const message =
        addDialog.querySelector<HTMLElement>('#general-error-message');
    assertTrue(!!message);
    generalError = message;

    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('[slot=title]');
    assertTrue(!!element);
    title = element.innerText;
    assertTrue(!!title);
  }

  // Sets |error| as error result for addAccount(), simulates a click on the
  // addAccount button and checks that |errorElement| has an non-empty
  // innerText value afterwards.
  async function checkAddAccountError(
      error: KerberosErrorType, errorElement: HTMLElement): Promise<void> {
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
  async function setConfig(config: string): Promise<void> {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);
    const configElement =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!configElement);
    assertFalse(configElement.disabled);
    configElement.value = config;
    const button =
        advancedConfigDialog.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!button);
    button.click();
    flush();
    await browserProxy.whenCalled('validateConfig');
  }

  // Opens the Advanced Config dialog, asserts that |config| is set as
  // Kerberos configuration and clicks 'Cancel'.
  async function assertConfig(config: string): Promise<void> {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);
    const configElement =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!configElement);
    assertEquals(config, configElement.value);
    const button =
        advancedConfigDialog.querySelector<CrButtonElement>('.cancel-button');
    assertTrue(!!button);
    button.click();
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
    await assertConfig(loadTimeData.getString('defaultKerberosConfig'));
    assertTrue(rememberPassword.checked);
  });

  // Verifies the rememberPassword state if no account is preset and password
  // should not be remembered by default.
  test('State without preset account and password not remembered', () => {
    loadTimeData.overrideValues({kerberosRememberPasswordByDefault: false});
    createDialog(null);

    assertFalse(rememberPassword.checked);
  });

  // Verifies expected state if an account is preset.
  test('State with preset account', async () => {
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!testAccount);
    createDialog(testAccount);
    assertTrue(title.startsWith('Refresh'));
    assertEquals('Refresh', actionButton.innerText);
    assertTrue(username.readonly);
    assertEquals(testAccount.principalName, username.value);
    await assertConfig(testAccount.config);
    // Password and remember password are tested below since the contents
    // depends on the passwordWasRemembered property of the account.
  });

  // The password input field is empty and 'Remember password' is checked if
  // |passwordWasRemembered| is false and the password should be remembered by
  // default.
  test('Password not preset if password was not remembered', () => {
    assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordByDefault'));
    const testAccount = TEST_KERBEROS_ACCOUNTS[0];
    assertTrue(!!testAccount);
    assertFalse(testAccount.passwordWasRemembered);
    createDialog(testAccount);

    assertEquals('', password.value);
    assertTrue(rememberPassword.checked);
  });

  // The password input field is empty and 'Remember password' is not checked if
  // |passwordWasRemembered| is false and the password should not be remembered
  // by default.
  test(
      'Checkbox unchecked if password was not remembered and feature disabled',
      () => {
        loadTimeData.overrideValues({kerberosRememberPasswordByDefault: false});
        createDialog(null);

        const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
        assertTrue(!!testAccount);
        assertFalse(testAccount.passwordWasRemembered);
        createDialog(testAccount);
        assertEquals('', password.value);
        assertFalse(rememberPassword.checked);
      });

  // The password input field is not empty and 'Remember password' is checked
  // if |passwordWasRemembered| is true.
  test('Password preset if password was remembered', () => {
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertTrue(testAccount.passwordWasRemembered);
    createDialog(testAccount);
    assertNotEquals('', password.value);
    assertTrue(rememberPassword.checked);
  });

  test('Remember password enabled', () => {
    assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordEnabled'));
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertTrue(testAccount.passwordWasRemembered);
    createDialog(testAccount);

    assertNull(
        dialog.shadowRoot!.querySelector('#rememberPasswordPolicyIndicator'));
    assertFalse(rememberPassword.disabled);
    assertTrue(rememberPassword.checked);
    assertNotEquals('', password.value);
  });

  test('Remember password disabled', () => {
    loadTimeData.overrideValues({kerberosRememberPasswordEnabled: false});
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertTrue(testAccount.passwordWasRemembered);
    createDialog(testAccount);

    assertTrue(
        !!dialog.shadowRoot!.querySelector('#rememberPasswordPolicyIndicator'));
    assertTrue(rememberPassword.disabled);
    assertFalse(rememberPassword.checked);
    assertEquals('', password.value);
  });

  test('Remember password visible and checked on user sessions', () => {
    assertFalse(loadTimeData.getBoolean('isGuest'));
    createDialog(null);

    const container = dialog.shadowRoot!.querySelector<HTMLElement>(
        '#rememberPasswordContainer');
    assertTrue(!!container);
    assertFalse(container.hidden);
    assertTrue(rememberPassword.checked);
  });

  test('Remember password hidden and not checked on MGS', () => {
    loadTimeData.overrideValues({isGuest: true});
    createDialog(null);

    const container = dialog.shadowRoot!.querySelector<HTMLElement>(
        '#rememberPasswordContainer');
    assertTrue(!!container);
    assertTrue(container.hidden);
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
    await setConfig(EXPECTED_CONFIG);
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
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    createDialog(testAccount);
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
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertTrue(testAccount.passwordWasRemembered);
    createDialog(testAccount);
    actionButton.click();
    const args = await browserProxy.whenCalled('addAccount');
    assertEquals('', args[AddParams.PASSWORD]);
    assertTrue(args[AddParams.REMEMBER_PASSWORD]);
  });

  // If the account has passwordWasRemembered === true and the user changes
  // the password before clicking the action button, the changed password is
  // submitted.
  test('Submits changed password if password field was changed', async () => {
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertTrue(testAccount.passwordWasRemembered);
    createDialog(testAccount);
    password.inputElement.value = 'some edit';
    password.dispatchEvent(new CustomEvent('input'));
    actionButton.click();
    const args = await browserProxy.whenCalled('addAccount');
    assertNotEquals('', args[AddParams.PASSWORD]);
    assertTrue(args[AddParams.REMEMBER_PASSWORD]);
  });

  test('Advanced config open and close', async () => {
    assertNull(dialog.shadowRoot!.querySelector('#advancedConfigDialog'));
    assertFalse(addDialog.hidden);
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();

    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector<CrDialogElement>(
            '#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);
    assertTrue(advancedConfigDialog.open);
    assertTrue(addDialog.hidden);
    const saveButton =
        advancedConfigDialog.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!saveButton);
    assertFalse(saveButton.disabled);
    saveButton.click();
    flush();
    assertTrue(saveButton.disabled);

    await browserProxy.whenCalled('validateConfig');
    flush();
    assertFalse(saveButton.disabled);
    assertNull(dialog.shadowRoot!.querySelector('#advancedConfigDialog'));
    assertFalse(addDialog.hidden);
    assertTrue(addDialog.open);
  });

  test('Advanced configuration save keeps config', async () => {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Change config and save.
    const modifiedConfig = 'modified';
    const config =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!config);
    config.value = modifiedConfig;
    const button =
        advancedConfigDialog.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!button);
    button.click();

    // Changed value should stick.
    await browserProxy.whenCalled('validateConfig');
    flush();
    await assertConfig(modifiedConfig);
  });

  test('Advanced configuration cancel resets config', async () => {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Change config and cancel.
    const config =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!config);
    const prevConfig = config.value;
    config.value = 'modified';
    const button =
        advancedConfigDialog.querySelector<CrButtonElement>('.cancel-button');
    assertTrue(!!button);
    button.click();
    flush();

    // Changed value should NOT stick.
    await assertConfig(prevConfig);
  });

  test('Advanced configuration disabled by policy', async () => {
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.THIRD];
    assertTrue(!!testAccount);
    assertTrue(testAccount.isManaged);
    createDialog(testAccount);
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);
    assertTrue(
        !!advancedConfigDialog.querySelector('#advancedConfigPolicyIndicator'));
    const config =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!config);
    assertTrue(config.disabled);
  });

  test('Advanced configuration validation error', async () => {
    advancedConfigButton.click();
    await browserProxy.whenCalled('validateConfig');
    flush();
    const advancedConfigDialog =
        dialog.shadowRoot!.querySelector<CrDialogElement>(
            '#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Cause a validation error.
    browserProxy.validateConfigResult = {
      error: KerberosErrorType.BAD_CONFIG,
      errorInfo:
          {code: KerberosConfigErrorCode.KEY_NOT_SUPPORTED, lineIndex: 0},
    };

    // Clicking the action button (aka 'Save') validates the config.
    let button =
        advancedConfigDialog.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!button);
    button.click();

    await browserProxy.whenCalled('validateConfig');

    // Wait for dialog to process the 'validateConfig' result (sets error
    // message etc.).
    await flushTasks();

    // Is some error text set?
    const configError = advancedConfigDialog.querySelector<HTMLElement>(
        '#config-error-message');
    assertTrue(!!configError);
    assertNotEquals(0, configError.innerText.length);

    // Is something selected?
    const configElement =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!configElement);
    const textArea = configElement.$.input;
    assertEquals(0, textArea.selectionStart);
    assertNotEquals(0, textArea.selectionEnd);

    // Is the config dialog still open?
    assertTrue(advancedConfigDialog.open);
    assertTrue(addDialog.hidden);

    // Was the config not accepted?
    button =
        advancedConfigDialog.querySelector<CrButtonElement>('.cancel-button');
    assertTrue(!!button);
    button.click();
    flush();
    await assertConfig(loadTimeData.getString('defaultKerberosConfig'));
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
        dialog.shadowRoot!.querySelector('#advancedConfigDialog');
    assertTrue(!!advancedConfigDialog);

    // Is some error text set?
    const configError = advancedConfigDialog.querySelector<HTMLElement>(
        '#config-error-message');
    assertTrue(!!configError);
    assertNotEquals(0, configError.innerText.length);

    // Is something selected?
    const configElement =
        advancedConfigDialog.querySelector<CrTextareaElement>('#config');
    assertTrue(!!configElement);
    const textArea = configElement.$.input;
    assertEquals(0, textArea.selectionStart);
    assertNotEquals(0, textArea.selectionEnd);
  });

  test('Domain autocomplete enabled', () => {
    loadTimeData.overrideValues({kerberosDomainAutocomplete: 'domain.com'});
    createDialog(null);

    // '@' should be automatically added to the policy value.
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#kerberosDomain');
    assertTrue(!!element);
    assertEquals('@domain.com', element.innerText);
  });

  test('Domain autocomplete enabled override', () => {
    loadTimeData.overrideValues({kerberosDomainAutocomplete: 'domain.com'});
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!testAccount);
    const principalName = testAccount.principalName;
    assertTrue(!!principalName);
    assertNotEquals(-1, principalName && principalName.indexOf('@'));
    createDialog(testAccount);

    // If inserted principal contains '@', nothing should be shown.
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#kerberosDomain');
    assertTrue(!!element);
    assertEquals('', element.innerText);
  });

  test('Domain autocomplete disabled', () => {
    assertEquals('', loadTimeData.getString('kerberosDomainAutocomplete'));
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#kerberosDomain');
    assertTrue(!!element);
    assertEquals('', element.innerText);
  });

  // Testing how `addAccount` error types are handled by the UI:

  // KerberosErrorType.PARSE_PRINCIPAL_FAILED spawns a username error.
  test('Add account error - parse principal failed', () => {
    checkAddAccountError(
        KerberosErrorType.PARSE_PRINCIPAL_FAILED, username.$.error);
  });

  // KerberosErrorType.BAD_PRINCIPAL spawns a username error.
  test('Add account error - bad principal', () => {
    checkAddAccountError(KerberosErrorType.BAD_PRINCIPAL, username.$.error);
  });

  // KerberosErrorType.DUPLICATE_PRINCIPAL_NAME spawns a username error.
  test('Add account error - duplicate principal name', () => {
    checkAddAccountError(
        KerberosErrorType.DUPLICATE_PRINCIPAL_NAME, username.$.error);
  });

  // KerberosErrorType.CONTACTING_KDC_FAILED spawns a username error.
  test('Add account error - contacting KDC failed', () => {
    checkAddAccountError(
        KerberosErrorType.CONTACTING_KDC_FAILED, username.$.error);
  });

  // KerberosErrorType.BAD_PASSWORD spawns a password error.
  test('Add account error - bad password', () => {
    checkAddAccountError(KerberosErrorType.BAD_PASSWORD, password.$.error);
  });

  // KerberosErrorType.PASSWORD_EXPIRED spawns a password error.
  test('Add account error - password expired', () => {
    checkAddAccountError(KerberosErrorType.PASSWORD_EXPIRED, password.$.error);
  });

  // KerberosErrorType.NETWORK_PROBLEM spawns a general error.
  test('Add account error - network problem', () => {
    checkAddAccountError(KerberosErrorType.NETWORK_PROBLEM, generalError);
  });

  // KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE spawns a general
  // error.
  test('Add account error - KDC does not support encryption type', () => {
    checkAddAccountError(
        KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE, generalError);
  });

  // KerberosErrorType.UNKNOWN spawns a general error.
  test('Add account error - unknown', () => {
    checkAddAccountError(KerberosErrorType.UNKNOWN, generalError);
  });

  // KerberosErrorType.BAD_CONFIG spawns a general error.
  test('Add account error - bad config', () => {
    checkAddAccountError(KerberosErrorType.BAD_CONFIG, generalError);
  });
});
