// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {KerberosAccountsBrowserProxyImpl, SettingsKerberosAccountsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, createRouterForTesting, CrToastElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {AccountIndex, TEST_KERBEROS_ACCOUNTS, TestKerberosAccountsBrowserProxy} from './test_kerberos_accounts_browser_proxy.js';

// Tests for the Kerberos Accounts settings subpage.
suite('<settings-kerberos-accounts-subpage>', () => {
  let browserProxy: TestKerberosAccountsBrowserProxy;
  let kerberosAccounts: SettingsKerberosAccountsSubpageElement;
  let accountList: DomRepeat;

  // Indices of 'More Actions' buttons.
  const MoreActions = {
    REFRESH_NOW: 0,
    SET_AS_ACTIVE_ACCOUNT: 1,
    REMOVE_ACCOUNT: 2,
  };

  suiteSetup(() => {
    // Reinitialize Router and routes based on load time data
    loadTimeData.overrideValues({isKerberosEnabled: true});

    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);
  });

  setup(() => {
    browserProxy = new TestKerberosAccountsBrowserProxy();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(browserProxy);

    createDialog();
  });

  teardown(() => {
    kerberosAccounts.remove();
  });

  function createDialog() {
    if (kerberosAccounts) {
      kerberosAccounts.remove();
    }

    kerberosAccounts =
        document.createElement('settings-kerberos-accounts-subpage');
    document.body.appendChild(kerberosAccounts);
    flush();

    const list =
        kerberosAccounts.shadowRoot!.querySelector<DomRepeat>('#account-list');
    assertTrue(!!list);
    accountList = list;
  }

  function clickMoreActions(accountIndex: number, moreActionsIndex: number) {
    // Click 'More actions' for the given account.
    const crButton =
        kerberosAccounts.shadowRoot!.querySelectorAll<CrButtonElement>(
            '.more-actions')[accountIndex];
    assertTrue(!!crButton);
    crButton.click();
    // Click on the given action.
    const menu = kerberosAccounts.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!menu);
    const button =
        menu.querySelectorAll<HTMLButtonElement>('button')[moreActionsIndex];
    assertTrue(!!button);
    button.click();
  }

  // Can be used to wait for event |eventName| on |element|, e.g.
  //   await onEvent(addDialog, 'close');
  function onEvent(element: HTMLElement, eventName: string): Promise<void> {
    return new Promise((resolve) => {
      element.addEventListener(eventName, function callback() {
        element.removeEventListener(eventName, callback);
        resolve();
      });
    });
  }

  test('Account list is populated at startup', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    // The test accounts were added in |getAccounts()| mock above.
    const items = accountList.items;
    assertTrue(!!items);
    assertEquals(TEST_KERBEROS_ACCOUNTS.length, items.length);
  });

  test('Account list signed-in signed-out labels', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    const accountListItem: NodeListOf<HTMLElement> =
        kerberosAccounts.shadowRoot!.querySelectorAll('.account-list-item');
    assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountListItem.length);

    // Show 'Valid for <duration>' for accounts that are signed in.
    let signedIn = accountListItem[0]!.querySelector<HTMLElement>('.signed-in');
    let signedOut =
        accountListItem[0]!.querySelector<HTMLElement>('.signed-out');
    let testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!signedIn);
    assertTrue(!!signedOut);
    assertTrue(!!testAccount);
    assertTrue(testAccount.isSignedIn);
    assertFalse(signedIn.hidden);
    assertTrue(signedOut.hidden);
    assertEquals(
        `Valid for ${testAccount.validForDuration}`, signedIn.innerText);

    // Show 'Expired' for accounts that are not signed in.
    signedIn = accountListItem[1]!.querySelector<HTMLElement>('.signed-in');
    signedOut = accountListItem[1]!.querySelector<HTMLElement>('.signed-out');
    testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!signedIn);
    assertTrue(!!signedOut);
    assertTrue(!!testAccount);
    assertFalse(testAccount.isSignedIn);
    assertTrue(signedIn.hidden);
    assertFalse(signedOut.hidden);
    assertEquals('Expired', signedOut.innerText);
  });

  test('Add account', () => {
    // The kerberos-add-account-dialog shouldn't be open yet.
    assertNull(kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog'));

    const button = kerberosAccounts.shadowRoot!.querySelector<CrButtonElement>(
        '#add-account-button');
    assertTrue(!!button);
    button.click();
    flush();

    const addDialog = kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals('', addDialog.$.username.value);
  });

  test('Reauthenticate account', async () => {
    // Wait until accounts are loaded.
    await browserProxy.whenCalled('getAccounts');
    flush();

    // The kerberos-add-account-dialog shouldn't be open yet.
    assertNull(kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog'));

    // Click "Sign-In" on an existing account.
    // Note that both accounts have a reauth button, but the first one is
    // hidden, so click the second one (clicking a hidden button works, but
    // it feels weird).
    const button =
        kerberosAccounts.shadowRoot!.querySelectorAll<CrButtonElement>(
            '.reauth-button')[AccountIndex.SECOND];
    assertTrue(!!button);
    button.click();
    flush();

    // Now the kerberos-add-account-dialog should be open with preset
    // username.
    const addDialog = kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertEquals(testAccount.principalName, addDialog.$.username.value);
  });

  // Appending '?kerberos_reauth=<principal>' to the URL opens the reauth
  // dialog for that account.
  test('Handle reauth query parameter', async () => {
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!testAccount);
    const principal_name = testAccount.principalName;
    const params = new URLSearchParams();
    params.append('kerberos_reauth', principal_name);
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2, params);

    // The flushTasks is necessary since the kerberos_reauth param would
    // otherwise be handled AFTER the callback below is executed.
    await browserProxy.whenCalled('getAccounts');
    await flushTasks();
    flush();
    const addDialog = kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals(principal_name, addDialog.$.username.value);
  });

  test('Refresh now', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REFRESH_NOW);
    flush();

    const addDialog = kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!testAccount);
    assertEquals(testAccount.principalName, addDialog.$.username.value);
  });

  test('Refresh account shows toast', async () => {
    let toast = kerberosAccounts.shadowRoot!.querySelector<CrToastElement>(
        '#account-toast');
    assertTrue(!!toast);
    assertFalse(toast.open);

    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REFRESH_NOW);
    flush();

    const addDialog = kerberosAccounts.shadowRoot!.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    const button =
        addDialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!button);
    button.click();
    flush();

    await onEvent(addDialog, 'close');
    await flushTasks();
    flush();
    toast = kerberosAccounts.shadowRoot!.querySelector<CrToastElement>(
        '#account-toast');
    assertTrue(!!toast);
    assertTrue(toast.open);
    const toastLabel =
        kerberosAccounts.shadowRoot!.querySelector('#account-toast-label');
    assertTrue(!!toastLabel);
    assertTrue(toastLabel.innerHTML.includes('refreshed'));
  });

  test('Remove account', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REMOVE_ACCOUNT);
    const account = await browserProxy.whenCalled('removeAccount');
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!testAccount);
    assertEquals(testAccount.principalName, account.principalName);
  });

  test('Deep link to remove account dropdown', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1801');
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2, params);

    await browserProxy.whenCalled('getAccounts');
    await flushTasks();
    flush();

    const deepLinkElement =
        kerberosAccounts.shadowRoot!.querySelectorAll('cr-icon-button')[0];
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Kebab menu should be focused for settingId=1801.');
  });

  test('Remove account shows toast', async () => {
    const toast = kerberosAccounts.shadowRoot!.querySelector<CrToastElement>(
        '#account-toast');
    assertTrue(!!toast);
    assertFalse(toast.open);

    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REMOVE_ACCOUNT);
    await browserProxy.whenCalled('removeAccount');
    await flushTasks();
    flush();
    assertTrue(toast.open);
    const toastLabel =
        kerberosAccounts.shadowRoot!.querySelector('#account-toast-label');
    assertTrue(!!toastLabel);
    assertTrue(toastLabel.innerHTML.includes('removed'));
  });

  test('Account list is updated when Kerberos accounts updates', () => {
    assertEquals(1, browserProxy.getCallCount('getAccounts'));
    webUIListenerCallback('kerberos-accounts-changed');
    assertEquals(2, browserProxy.getCallCount('getAccounts'));
  });

  test('Set as active account', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.SECOND, MoreActions.SET_AS_ACTIVE_ACCOUNT);
    const account = await browserProxy.whenCalled('setAsActiveAccount');
    const testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND];
    assertTrue(!!testAccount);
    assertEquals(testAccount.principalName, account.principalName);
  });

  test('Show policy indicator for managed accounts', async () => {
    // Make sure we have at least one managed and one unmanaged account.
    let testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST];
    assertTrue(!!testAccount);
    assertFalse(testAccount.isManaged);
    testAccount = TEST_KERBEROS_ACCOUNTS[AccountIndex.THIRD];
    assertTrue(!!testAccount);
    assertTrue(testAccount.isManaged);

    await browserProxy.whenCalled('getAccounts');
    flush();
    const accountListItem: NodeListOf<HTMLElement> =
        kerberosAccounts.shadowRoot!.querySelectorAll('.account-list-item');
    assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountListItem.length);

    for (let i = 0; i < TEST_KERBEROS_ACCOUNTS.length; i++) {
      // Assert account has policy indicator iff account is managed.
      const hasAccountPolicyIndicator =
          !!accountListItem[i]!.querySelector('.account-policy-indicator');
      assertEquals(
          TEST_KERBEROS_ACCOUNTS[i]!.isManaged, hasAccountPolicyIndicator);

      // Assert 'Remove' button is disabled iff account is managed.
      const button =
          accountListItem[i]!.querySelector<CrButtonElement>('.more-actions');
      assertTrue(!!button);
      button.click();
      let menu = kerberosAccounts.shadowRoot!.querySelector('cr-action-menu');
      assertTrue(!!menu);
      const moreActions = menu.querySelectorAll('button');
      assertTrue(!!moreActions);
      const removeAccountButton = moreActions[MoreActions.REMOVE_ACCOUNT];
      assertTrue(!!removeAccountButton);
      assertEquals(
          TEST_KERBEROS_ACCOUNTS[i]!.isManaged, removeAccountButton.disabled);

      // Assert 'Remove' button has policy indicator iff account is managed.
      flush();
      const hasRemovalPolicyIndicator = !!removeAccountButton.querySelector(
          '#remove-account-policy-indicator');
      assertEquals(
          TEST_KERBEROS_ACCOUNTS[i]!.isManaged, hasRemovalPolicyIndicator);

      menu = kerberosAccounts.shadowRoot!.querySelector('cr-action-menu');
      assertTrue(!!menu);
      menu.close();
    }
  });

  test('Add accounts allowed', () => {
    assertTrue(loadTimeData.getBoolean('kerberosAddAccountsAllowed'));
    createDialog();

    assertNull(kerberosAccounts.shadowRoot!.querySelector(
        '#add-account-policy-indicator'));
    const button = kerberosAccounts.shadowRoot!.querySelector<CrButtonElement>(
        '#add-account-button');
    assertTrue(!!button);
    assertFalse(button.disabled);
  });

  test('Add accounts not allowed', () => {
    loadTimeData.overrideValues({kerberosAddAccountsAllowed: false});
    createDialog();

    assertTrue(!!kerberosAccounts.shadowRoot!.querySelector(
        '#add-account-policy-indicator'));
    const button = kerberosAccounts.shadowRoot!.querySelector<CrButtonElement>(
        '#add-account-button');
    assertTrue(!!button);
    assertTrue(button.disabled);
  });
});
