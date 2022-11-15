// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccountManagerBrowserProxy} from 'chrome://account-migration-welcome/account_manager_browser_proxy.js';

import {TestBrowserProxy} from 'chrome://webui-test/chromeos/test_browser_proxy.js';

/** @implements {AccountManagerBrowserProxy} */
export class TestAccountManagerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'closeDialog',
      'reauthenticateAccount',
    ]);
  }

  /** @override */
  closeDialog() {
    this.methodCalled('closeDialog');
  }

  /** @override */
  reauthenticateAccount(account_email) {
    this.methodCalled('reauthenticateAccount', account_email);
  }
}
