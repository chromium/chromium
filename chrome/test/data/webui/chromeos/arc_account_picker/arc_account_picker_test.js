// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArcAccountPickerAppElement} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_app.js';
import {Account, ArcAccountPickerBrowserProxyImpl} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_browser_proxy.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {getFakeAccountsNotAvailableInArcList, TestArcAccountPickerBrowserProxy} from './test_util.js';

window.arc_account_picker_test = {};
const arc_account_picker_test = window.arc_account_picker_test;
arc_account_picker_test.suiteName = 'ArcAccountPickerTest';

/** @enum {string} */
arc_account_picker_test.TestNames = {
  EmptyAccountList: 'EmptyAccountList',
  AccountList: 'AccountList',
  AddAccount: 'AddAccount',
  MakeAvailableInArc: 'MakeAvailableInArc',
  LinkClick: 'LinkClick',
};

suite(arc_account_picker_test.suiteName, () => {
  /** @type {ArcAccountPickerAppElement} */
  let arcAccountPickerComponent;
  /** @type {TestArcAccountPickerBrowserProxy} */
  let testBrowserProxy;

  /**
   * @param {!Array<Account>} accountsNotAvailableInArc
   */
  function testSetup(accountsNotAvailableInArc) {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    testBrowserProxy = new TestArcAccountPickerBrowserProxy();
    if (accountsNotAvailableInArc) {
      testBrowserProxy.setAccountsNotAvailableInArc(accountsNotAvailableInArc);
    }
    ArcAccountPickerBrowserProxyImpl.setInstance(testBrowserProxy);

    arcAccountPickerComponent = /** @type {!ArcAccountPickerAppElement} */ (
        document.createElement('arc-account-picker-app'));
    document.body.appendChild(arcAccountPickerComponent);
    flush();
  }

  test(assert(arc_account_picker_test.TestNames.EmptyAccountList), async () => {
    testSetup([]);

    const accountsFound = await arcAccountPickerComponent.loadAccounts();
    assertFalse(accountsFound);
    flush();
    const uiAccounts = [
      ...arcAccountPickerComponent.shadowRoot.querySelectorAll('.account-item'),
    ].filter(item => item.id !== 'addAccountButton');
    assertEquals(0, uiAccounts.length);
  });


  test(assert(arc_account_picker_test.TestNames.AccountList), async () => {
    const fakeAccounts = getFakeAccountsNotAvailableInArcList();
    testSetup(fakeAccounts);

    const accountsFound = await arcAccountPickerComponent.loadAccounts();
    assertTrue(accountsFound);
    flush();
    const uiAccounts = [
      ...arcAccountPickerComponent.shadowRoot.querySelectorAll('.account-item'),
    ].filter(item => item.id !== 'addAccountButton');
    assertEquals(fakeAccounts.length, uiAccounts.length);
  });

  test(assert(arc_account_picker_test.TestNames.AddAccount), async () => {
    testSetup(getFakeAccountsNotAvailableInArcList());
    await arcAccountPickerComponent.loadAccounts();
    flush();

    const addAccountPromise = new Promise(
        resolve => arcAccountPickerComponent.addEventListener(
            'add-account', () => resolve()));
    arcAccountPickerComponent.shadowRoot.querySelector('#addAccountButton')
        .click();
    await addAccountPromise;
  });

  test(
      assert(arc_account_picker_test.TestNames.MakeAvailableInArc),
      async () => {
        const fakeAccounts = getFakeAccountsNotAvailableInArcList();
        testSetup(fakeAccounts);
        await arcAccountPickerComponent.loadAccounts();
        flush();

        const expectedAccount = fakeAccounts[1];
        arcAccountPickerComponent.shadowRoot
            .querySelectorAll('.account-item')[1]
            .click();
        const account = await testBrowserProxy.whenCalled('makeAvailableInArc');
        assertDeepEquals(expectedAccount, account);
      });

  test(assert(arc_account_picker_test.TestNames.LinkClick), async () => {
    testSetup(getFakeAccountsNotAvailableInArcList());
    await arcAccountPickerComponent.loadAccounts();
    flush();

    const newWindowPromise = new Promise(
        resolve => arcAccountPickerComponent.addEventListener(
            'opened-new-window', () => resolve()));
    arcAccountPickerComponent.shadowRoot.querySelector('#osSettingsLink')
        .click();
    await newWindowPromise;
  });
});
