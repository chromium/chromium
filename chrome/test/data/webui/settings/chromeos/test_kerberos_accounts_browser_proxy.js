// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KerberosConfigErrorCode, KerberosErrorType} from 'chrome://os-settings/chromeos/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// List of fake accounts.
export const TEST_KERBEROS_ACCOUNTS = [
  {
    principalName: 'user@REALM',
    config: 'config1',
    isSignedIn: true,
    isActive: true,
    isManaged: false,
    passwordWasRemembered: false,
    pic: 'pic',
    validForDuration: '1 lightyear',
  },
  {
    principalName: 'user2@REALM2',
    config: 'config2',
    isSignedIn: false,
    isActive: false,
    isManaged: false,
    passwordWasRemembered: true,
    pic: 'pic2',
    validForDuration: 'zero googolseconds',
  },
  {
    principalName: 'user3@REALM3',
    config: 'config3',
    isSignedIn: false,
    isActive: false,
    isManaged: true,
    passwordWasRemembered: true,
    pic: 'pic2',
    validForDuration: 'one over inf seconds',
  },
];

/** @implements {KerberosAccountsBrowserProxy} */
export class TestKerberosAccountsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccounts',
      'addAccount',
      'removeAccount',
      'validateConfig',
      'setAsActiveAccount',
    ]);

    // Simulated error from an addAccount call.
    this.addAccountError = KerberosErrorType.NONE;

    // Simulated error from a validateConfig call.
    this.validateConfigResult = {
      error: KerberosErrorType.NONE,
      errorInfo: {code: KerberosConfigErrorCode.NONE},
    };
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');
    return Promise.resolve(TEST_KERBEROS_ACCOUNTS);
  }

  /** @override */
  addAccount(principalName, password, rememberPassword, config, allowExisting) {
    this.methodCalled(
        'addAccount',
        [principalName, password, rememberPassword, config, allowExisting]);
    return Promise.resolve(this.addAccountError);
  }

  /** @override */
  removeAccount(account) {
    this.methodCalled('removeAccount', account);
    return Promise.resolve(KerberosErrorType.NONE);
  }

  /** @override */
  validateConfig(account) {
    this.methodCalled('validateConfig', account);
    return Promise.resolve(this.validateConfigResult);
  }

  /** @override */
  setAsActiveAccount(account) {
    this.methodCalled('setAsActiveAccount', account);
  }
}
