// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Account, ArcAccountPickerBrowserProxy, ArcAccountPickerBrowserProxyImpl} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/chromeos/test_browser_proxy.js';

/**
 * @param {!TestArcAccountPickerBrowserProxy} testBrowserProxy
 */
export function setTestArcAccountPickerBrowserProxy(testBrowserProxy) {
  ArcAccountPickerBrowserProxyImpl.setInstance(
      /** @type {!ArcAccountPickerBrowserProxy} */ (testBrowserProxy));
}

/** @return {!Array<Account>} */
export function getFakeAccountsNotAvailableInArcList() {
  return [
    {
      id: '1',
      email: 'test@gmail.com',
      fullName: 'Test User',
      image: 'data:image/png;base64,abc123',
    },
    {id: '2', email: 'test2@gmail.com', fullName: 'Test2 User', image: ''},
    {id: '3', email: 'test3@gmail.com', fullName: 'Test3 User', image: ''},
  ];
}

/** @implements {ArcAccountPickerBrowserProxy} */
export class TestArcAccountPickerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccountsNotAvailableInArc',
      'makeAvailableInArc',
    ]);

    /** @private */
    this.accountsNotAvailableInArc_ = [];
  }

  /**
   * @param {!Array<Account>} accountsNotAvailableInArc
   */
  setAccountsNotAvailableInArc(accountsNotAvailableInArc) {
    this.accountsNotAvailableInArc_ = accountsNotAvailableInArc;
  }

  /** @override */
  getAccountsNotAvailableInArc() {
    this.methodCalled('getAccountsNotAvailableInArc');
    return Promise.resolve(this.accountsNotAvailableInArc_);
  }

  /** @override */
  makeAvailableInArc(account) {
    this.methodCalled('makeAvailableInArc', account);
  }
}
