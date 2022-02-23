// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {Account, InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals} from '../chai_assert.js';

import {fakeAuthExtensionData, fakeAuthExtensionDataWithEmail, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

window.arc_account_picker_page_test = {};
const arc_account_picker_page_test = window.arc_account_picker_page_test;
arc_account_picker_page_test.suiteName = 'InlineLoginWelcomePageTest';

/** @enum {string} */
arc_account_picker_page_test.TestNames = {
  ArcPickerActive: 'ArcPickerActive',
  ArcPickerHiddenForReauth: 'ArcPickerHiddenForReauth',
  ArcPickerHiddenNoAccounts: 'ArcPickerHiddenNoAccounts',
  AddAccount: 'AddAccount',
  MakeAvailableInArc: 'MakeAvailableInArc',
};

/** @return {!Array<Account>} */
function getFakeAccountsNotAvailableInArcList() {
  return [
    {
      id: '1',
      email: 'test@gmail.com',
      fullName: 'Test User',
      image: 'data:image/png;base64,abc123'
    },
    {id: '2', email: 'test2@gmail.com', fullName: 'Test2 User', image: ''},
    {id: '3', email: 'test3@gmail.com', fullName: 'Test3 User', image: ''},
  ];
}

suite(arc_account_picker_page_test.suiteName, () => {
  /** @type {ArcAccountAppPickerAppElement} */
  let arcAccountPickerComponent;
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;

  /**
   * @return {string} id of the active view.
   */
  function getActiveViewId() {
    return inlineLoginComponent.$$('div.active[slot="view"]').id;
  }

  /**
   * @param {?AccountAdditionOptions} dialogArgs
   * @param {?Array<Account>} accountsNotAvailableInArc
   */
  function testSetup(dialogArgs, accountsNotAvailableInArc) {
    document.body.innerHTML = '';
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    if (accountsNotAvailableInArc) {
      testBrowserProxy.setAccountsNotAvailableInArc(accountsNotAvailableInArc);
    }
    InlineLoginBrowserProxyImpl.instance_ = testBrowserProxy;
    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    inlineLoginComponent.setAuthExtHostForTest(new TestAuthenticator());
    flush();
    arcAccountPickerComponent =
        inlineLoginComponent.$$('arc-account-picker-app');
  }

  test(
      assert(arc_account_picker_page_test.TestNames.ArcPickerActive),
      async () => {
        testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            getFakeAccountsNotAvailableInArcList());
        // Send auth extension data without email -> it's account addition flow.
        webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
        // Wait for getAccountsNotAvailableInArc call which will return > 0
        // accounts.
        await testBrowserProxy.whenCalled('getAccountsNotAvailableInArc');
        flush();
        assertEquals(
            inlineLoginComponent.View.arcAccountPicker, getActiveViewId(),
            'ARC account picker screen should be active');

        const uiAccounts = [
          ...arcAccountPickerComponent.root.querySelectorAll('.account-item')
        ].filter(item => item.id !== 'addAccountButton');
        assertEquals(
            getFakeAccountsNotAvailableInArcList().length, uiAccounts.length);
      });

  test(
      assert(arc_account_picker_page_test.TestNames.ArcPickerHiddenForReauth),
      () => {
        testSetup({isAvailableInArc: true, showArcAvailabilityPicker: true});
        // Send auth extension data with email -> it's reauthentication flow.
        webUIListenerCallback(
            'load-auth-extension', fakeAuthExtensionDataWithEmail);
        assertEquals(
            inlineLoginComponent.View.addAccount, getActiveViewId(),
            'Add account view should be active for reauthentication');
      });

  test(
      assert(arc_account_picker_page_test.TestNames.ArcPickerHiddenNoAccounts),
      async () => {
        testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            /*accountsNotAvailableInArc=*/[]);
        // Send auth extension data without email -> it's account addition flow.
        webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
        // Wait for getAccountsNotAvailableInArc call which will return 0
        // accounts.
        return testBrowserProxy.whenCalled('getAccountsNotAvailableInArc')
            .then(function() {
              assertEquals(
                  inlineLoginComponent.View.welcome, getActiveViewId(),
                  'Welcome view should be active when there are 0 accounts' +
                      ' not available in ARC');
            });
      });

  test(assert(arc_account_picker_page_test.TestNames.AddAccount), async () => {
    testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        getFakeAccountsNotAvailableInArcList());
    // Send auth extension data without email -> it's account addition flow.
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    // Wait for getAccountsNotAvailableInArc call which will return > 0
    // accounts.
    await testBrowserProxy.whenCalled('getAccountsNotAvailableInArc');
    flush();
    assertEquals(
        inlineLoginComponent.View.arcAccountPicker, getActiveViewId(),
        'ARC account picker screen should be active');

    arcAccountPickerComponent.shadowRoot.querySelector('#addAccountButton')
        .click();
    assertEquals(
        inlineLoginComponent.View.welcome, getActiveViewId(),
        'Welcome screen should be active after Add account button click');
  });

  test(
      assert(arc_account_picker_page_test.TestNames.MakeAvailableInArc),
      async () => {
        testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            getFakeAccountsNotAvailableInArcList());
        // Send auth extension data without email -> it's account addition flow.
        webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
        // Wait for getAccountsNotAvailableInArc call which will return > 0
        // accounts.
        await testBrowserProxy.whenCalled('getAccountsNotAvailableInArc');
        flush();
        assertEquals(
            inlineLoginComponent.View.arcAccountPicker, getActiveViewId(),
            'ARC account picker screen should be active');

        const expectedAccount = getFakeAccountsNotAvailableInArcList()[0];
        arcAccountPickerComponent.shadowRoot
            .querySelectorAll('.account-item')[0]
            .click();
        return testBrowserProxy.whenCalled('makeAvailableInArc')
            .then(function(account) {
              assertDeepEquals(expectedAccount, account);
            });
      });
});
