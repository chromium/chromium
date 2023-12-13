// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {ArcAccountPickerAppElement} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_app.js';
import {Account} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_browser_proxy.js';
import {AccountAdditionOptions} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import {InlineLoginAppElement, View} from 'chrome://chrome-signin/inline_login_app.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {getFakeAccountsNotAvailableInArcList, setTestArcAccountPickerBrowserProxy, TestArcAccountPickerBrowserProxy} from 'chrome://webui-test/chromeos/arc_account_picker/test_util.js';

import {fakeAuthenticationData, fakeAuthenticationDataWithEmail, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

suite('InlineLoginArcPickerPageTest', () => {
  let arcAccountPickerComponent: ArcAccountPickerAppElement;
  let inlineLoginComponent: InlineLoginAppElement;
  let testBrowserProxy: TestInlineLoginBrowserProxy;
  let testArcBrowserProxy: TestArcAccountPickerBrowserProxy;

  /** @return id of the active view. */
  function getActiveViewId(): string {
    return inlineLoginComponent.shadowRoot!
        .querySelector('div.active[slot="view"]')!.id;
  }

  async function testSetup(
      dialogArgs: AccountAdditionOptions|null,
      accountsNotAvailableInArc: Account[], authenticationData: object) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    InlineLoginBrowserProxyImpl.setInstance(testBrowserProxy);

    testArcBrowserProxy = new TestArcAccountPickerBrowserProxy();
    testArcBrowserProxy.setAccountsNotAvailableInArc(accountsNotAvailableInArc);
    setTestArcAccountPickerBrowserProxy(testArcBrowserProxy);

    inlineLoginComponent = document.createElement('inline-login-app');
    document.body.appendChild(inlineLoginComponent);
    inlineLoginComponent.setAuthenticatorForTest(new TestAuthenticator());
    flush();
    arcAccountPickerComponent = inlineLoginComponent.shadowRoot!.querySelector(
        'arc-account-picker-app')!;

    const switchViewPromise = new Promise<void>(resolve => {
      inlineLoginComponent.addEventListener(
          'switch-view-notify-for-testing', () => resolve());
    });
    webUIListenerCallback('load-authenticator', authenticationData);
    await switchViewPromise;
    flush();
  }

  test('ArcPickerActive', async () => {
    await testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        getFakeAccountsNotAvailableInArcList(),
        // Send authentication data without email -> it's account addition
        // flow.
        fakeAuthenticationData);
    assertEquals(
        View.ARC_ACCOUNT_PICKER, getActiveViewId(),
        'ARC account picker screen should be active');

    const uiAccounts = [
      ...arcAccountPickerComponent.shadowRoot!.querySelectorAll(
          '.account-item'),
    ].filter(item => item.id !== 'addAccountButton');
    assertEquals(
        getFakeAccountsNotAvailableInArcList().length, uiAccounts.length);
  });

  test('ArcPickerHiddenForReauth', async () => {
    await testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        /*accountsNotAvailableInArc=*/[],
        // Send authentication data with email -> it's reauthentication
        // flow.
        fakeAuthenticationDataWithEmail);
    assertEquals(
        View.ADD_ACCOUNT, getActiveViewId(),
        'Add account view should be active for reauthentication');
  });

  test('ArcPickerHiddenNoAccounts', async () => {
    await testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        /*accountsNotAvailableInArc=*/[],
        // Send auth authentication without email -> it's account addition
        // flow.
        fakeAuthenticationData);
    assertEquals(
        View.WELCOME, getActiveViewId(),
        'Welcome view should be active when there are 0 accounts' +
            ' not available in ARC');
  });

  test('AddAccount', async () => {
    await testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        getFakeAccountsNotAvailableInArcList(),
        // Send authentication data without email -> it's account addition flow.
        fakeAuthenticationData);
    assertEquals(
        View.ARC_ACCOUNT_PICKER, getActiveViewId(),
        'ARC account picker screen should be active');

    const button =
        arcAccountPickerComponent.shadowRoot!.querySelector<HTMLElement>(
            '#addAccountButton');
    assertTrue(!!button);
    button.click();
    assertEquals(
        View.WELCOME, getActiveViewId(),
        'Welcome screen should be active after Add account button click');
  });

  test('MakeAvailableInArc', async () => {
    await testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        getFakeAccountsNotAvailableInArcList(),
        // Send authentication data without email -> it's account addition
        // flow.
        fakeAuthenticationData);
    assertEquals(
        View.ARC_ACCOUNT_PICKER, getActiveViewId(),
        'ARC account picker screen should be active');

    const expectedAccount = getFakeAccountsNotAvailableInArcList()[0];
    const accountItem =
        arcAccountPickerComponent.shadowRoot!.querySelector<HTMLElement>(
            '.account-item');
    assertTrue(!!accountItem);
    accountItem.click();
    const account = await testArcBrowserProxy.whenCalled('makeAvailableInArc');
    assertDeepEquals(expectedAccount, account);
  });
});
