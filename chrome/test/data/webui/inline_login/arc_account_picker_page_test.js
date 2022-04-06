// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {ArcAccountPickerAppElement} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_app.js';
import {Account} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_browser_proxy.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {AccountAdditionOptions} from 'chrome://chrome-signin/inline_login_util.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals} from '../chai_assert.js';
import {getFakeAccountsNotAvailableInArcList, setTestArcAccountPickerBrowserProxy, TestArcAccountPickerBrowserProxy} from '../chromeos/arc_account_picker/test_util.js';

import {fakeAuthExtensionData, fakeAuthExtensionDataWithEmail, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

window.arc_account_picker_page_test = {};
const arc_account_picker_page_test = window.arc_account_picker_page_test;
arc_account_picker_page_test.suiteName = 'InlineLoginArcPickerPageTest';

/** @enum {string} */
arc_account_picker_page_test.TestNames = {
  ArcPickerActive: 'ArcPickerActive',
  ArcPickerHiddenForReauth: 'ArcPickerHiddenForReauth',
  ArcPickerHiddenNoAccounts: 'ArcPickerHiddenNoAccounts',
  AddAccount: 'AddAccount',
  MakeAvailableInArc: 'MakeAvailableInArc',
};

suite(arc_account_picker_page_test.suiteName, () => {
  /** @type {ArcAccountPickerAppElement} */
  let arcAccountPickerComponent;
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;
  /** @type {TestArcAccountPickerBrowserProxy} */
  let testArcBrowserProxy;

  /**
   * @return {string} id of the active view.
   */
  function getActiveViewId() {
    return inlineLoginComponent.$$('div.active[slot="view"]').id;
  }

  /**
   * @param {?AccountAdditionOptions} dialogArgs
   * @param {!Array<Account>} accountsNotAvailableInArc
   * @param {!Object} authExtensionData
   */
  async function testSetup(
      dialogArgs, accountsNotAvailableInArc, authExtensionData) {
    document.body.innerHTML = '';

    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    InlineLoginBrowserProxyImpl.instance_ = testBrowserProxy;

    testArcBrowserProxy = new TestArcAccountPickerBrowserProxy();
    testArcBrowserProxy.setAccountsNotAvailableInArc(accountsNotAvailableInArc);
    setTestArcAccountPickerBrowserProxy(testArcBrowserProxy);

    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    inlineLoginComponent.setAuthExtHostForTest(new TestAuthenticator());
    flush();
    arcAccountPickerComponent = /** @type {ArcAccountPickerAppElement} */ (
        inlineLoginComponent.$$('arc-account-picker-app'));

    const switchViewPromise = new Promise(
        resolve => inlineLoginComponent.addEventListener(
            'switch-view-notify-for-testing', () => resolve()));
    webUIListenerCallback('load-auth-extension', authExtensionData);
    await switchViewPromise;
    flush();
  }

  test(
      assert(arc_account_picker_page_test.TestNames.ArcPickerActive),
      async () => {
        await testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            getFakeAccountsNotAvailableInArcList(),
            // Send auth extension data without email -> it's account addition
            // flow.
            fakeAuthExtensionData);
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
      async () => {
        await testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            /*accountsNotAvailableInArc=*/[],
            // Send auth extension data with email -> it's reauthentication
            // flow.
            fakeAuthExtensionDataWithEmail);
        assertEquals(
            inlineLoginComponent.View.addAccount, getActiveViewId(),
            'Add account view should be active for reauthentication');
      });

  test(
      assert(arc_account_picker_page_test.TestNames.ArcPickerHiddenNoAccounts),
      async () => {
        await testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            /*accountsNotAvailableInArc=*/[],
            // Send auth extension data without email -> it's account addition
            // flow.
            fakeAuthExtensionData);
        assertEquals(
            inlineLoginComponent.View.welcome, getActiveViewId(),
            'Welcome view should be active when there are 0 accounts' +
                ' not available in ARC');
      });

  test(assert(arc_account_picker_page_test.TestNames.AddAccount), async () => {
    await testSetup(
        {isAvailableInArc: true, showArcAvailabilityPicker: true},
        getFakeAccountsNotAvailableInArcList(),
        // Send auth extension data without email -> it's account addition flow.
        fakeAuthExtensionData);
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
        await testSetup(
            {isAvailableInArc: true, showArcAvailabilityPicker: true},
            getFakeAccountsNotAvailableInArcList(),
            // Send auth extension data without email -> it's account addition
            // flow.
            fakeAuthExtensionData);
        assertEquals(
            inlineLoginComponent.View.arcAccountPicker, getActiveViewId(),
            'ARC account picker screen should be active');

        const expectedAccount = getFakeAccountsNotAvailableInArcList()[0];
        arcAccountPickerComponent.shadowRoot
            .querySelectorAll('.account-item')[0]
            .click();
        const account =
            await testArcBrowserProxy.whenCalled('makeAvailableInArc');
        assertDeepEquals(expectedAccount, account);
      });
});
