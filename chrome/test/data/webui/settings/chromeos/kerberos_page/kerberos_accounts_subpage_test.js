// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import 'chrome://os-settings/lazy_load.js';

import {Router, Route, routes} from 'chrome://os-settings/os_settings.js';
import {KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType} from 'chrome://os-settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {AccountIndex, TestKerberosAccountsBrowserProxy, TEST_KERBEROS_ACCOUNTS} from './test_kerberos_accounts_browser_proxy.js';

// Tests for the Kerberos Accounts settings subpage.
suite('Kerberos accounts subpage tests', function() {
  let browserProxy = null;
  let kerberosAccounts = null;
  let accountList = null;

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

    // Setting the default value of the relevant load time data.
    loadTimeData.overrideValues({kerberosAddAccountsAllowed: true});

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

    kerberosAccounts =
        document.createElement('settings-kerberos-accounts-subpage');
    document.body.appendChild(kerberosAccounts);
    flush();

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

  test('Account list is populated at startup', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    // The test accounts were added in |getAccounts()| mock above.
    assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountList.items.length);
  });

  test('Account list signed-in signed-out labels', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    accountList =
        kerberosAccounts.shadowRoot.querySelectorAll('.account-list-item');
    assertEquals(TEST_KERBEROS_ACCOUNTS.length, accountList.length);

    // Show 'Valid for <duration>' for accounts that are signed in.
    let signedIn = accountList[0].querySelector('.signed-in');
    let signedOut = accountList[0].querySelector('.signed-out');
    assertTrue(TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].isSignedIn);
    assertFalse(signedIn.hidden);
    assertTrue(signedOut.hidden);
    assertEquals(
        'Valid for ' +
            TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].validForDuration,
        signedIn.innerText);

    // Show 'Expired' for accounts that are not signed in.
    signedIn = accountList[1].querySelector('.signed-in');
    signedOut = accountList[1].querySelector('.signed-out');
    assertFalse(TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].isSignedIn);
    assertTrue(signedIn.hidden);
    assertFalse(signedOut.hidden);
    assertEquals('Expired', signedOut.innerText);
  });

  test('Add account', function() {
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

  test('Reauthenticate account', async () => {
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
        .querySelectorAll('.reauth-button')[AccountIndex.SECOND]
        .click();
    flush();

    // Now the kerberos-add-account-dialog should be open with preset
    // username.
    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].principalName,
        addDialog.$.username.value);
  });

  // Appending '?kerberos_reauth=<principal>' to the URL opens the reauth
  // dialog for that account.
  test('Handle reauth query parameter', async () => {
    const principal_name =
        TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].principalName;
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

  test('Refresh now', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REFRESH_NOW);
    flush();

    const addDialog = kerberosAccounts.shadowRoot.querySelector(
        'kerberos-add-account-dialog');
    assertTrue(!!addDialog);
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].principalName,
        addDialog.$.username.value);
  });

  test('Refresh account shows toast', async () => {
    const toast = kerberosAccounts.shadowRoot.querySelector('#account-toast');
    assertTrue(!!toast);
    assertFalse(toast.open);

    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REFRESH_NOW);
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

  test('Remove account', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REMOVE_ACCOUNT);
    const account = await browserProxy.whenCalled('removeAccount');
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].principalName,
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

  test('Remove account shows toast', async () => {
    const toast = kerberosAccounts.shadowRoot.querySelector('#account-toast');
    assertTrue(!!toast);
    assertFalse(toast.open);

    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.FIRST, MoreActions.REMOVE_ACCOUNT);
    await browserProxy.whenCalled('removeAccount');
    await flushTasks();
    flush();
    assertTrue(toast.open);
    assertTrue(kerberosAccounts.shadowRoot.querySelector('#account-toast-label')
                   .innerHTML.includes('removed'));
  });

  test('Account list is updated when Kerberos accounts updates', function() {
    assertEquals(1, browserProxy.getCallCount('getAccounts'));
    webUIListenerCallback('kerberos-accounts-changed');
    assertEquals(2, browserProxy.getCallCount('getAccounts'));
  });

  test('Set as active account', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();
    clickMoreActions(AccountIndex.SECOND, MoreActions.SET_AS_ACTIVE_ACCOUNT);
    const account = await browserProxy.whenCalled('setAsActiveAccount');
    assertEquals(
        TEST_KERBEROS_ACCOUNTS[AccountIndex.SECOND].principalName,
        account.principalName);
  });

  test('Show policy indicator for managed accounts', async () => {
    // Make sure we have at least one managed and one unmanaged account.
    assertFalse(TEST_KERBEROS_ACCOUNTS[AccountIndex.FIRST].isManaged);
    assertTrue(TEST_KERBEROS_ACCOUNTS[AccountIndex.THIRD].isManaged);

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

  test('Add accounts allowed', function() {
    assertTrue(loadTimeData.getBoolean('kerberosAddAccountsAllowed'));
    createDialog();

    assertTrue(!kerberosAccounts.shadowRoot.querySelector(
        '#add-account-policy-indicator'));
    assertFalse(kerberosAccounts.shadowRoot.querySelector('#add-account-button')
                    .disabled);
  });

  test('Add accounts not allowed', function() {
    loadTimeData.overrideValues({kerberosAddAccountsAllowed: false});
    createDialog();

    assertTrue(!!kerberosAccounts.shadowRoot.querySelector(
        '#add-account-policy-indicator'));
    assertTrue(kerberosAccounts.shadowRoot.querySelector('#add-account-button')
                   .disabled);
  });
});
