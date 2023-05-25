// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KerberosAccount, KerberosAccountsBrowserProxy, KerberosConfigErrorCode, KerberosErrorType, ValidateKerberosConfigResult} from 'chrome://os-settings/lazy_load.js';
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

// Account indices (to help readability).
export const AccountIndex = {
  FIRST: 0,
  SECOND: 1,
  THIRD: 2,
};

export class TestKerberosAccountsBrowserProxy extends TestBrowserProxy
    implements KerberosAccountsBrowserProxy {
  addAccountError: KerberosErrorType;
  validateConfigResult: ValidateKerberosConfigResult;

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
      errorInfo: {code: KerberosConfigErrorCode.NONE, lineIndex: 0},
    };
  }

  getAccounts(): Promise<KerberosAccount[]> {
    this.methodCalled('getAccounts');
    return Promise.resolve(TEST_KERBEROS_ACCOUNTS);
  }

  addAccount(
      principalName: string, password: string, rememberPassword: boolean,
      config: string, allowExisting: boolean): Promise<KerberosErrorType> {
    this.methodCalled(
        'addAccount',
        [principalName, password, rememberPassword, config, allowExisting]);
    return Promise.resolve(this.addAccountError);
  }

  removeAccount(account: KerberosAccount): Promise<KerberosErrorType> {
    this.methodCalled('removeAccount', account);
    return Promise.resolve(KerberosErrorType.NONE);
  }

  validateConfig(krb5Conf: string): Promise<ValidateKerberosConfigResult> {
    this.methodCalled('validateConfig', krb5Conf);
    return Promise.resolve(this.validateConfigResult);
  }

  setAsActiveAccount(account: KerberosAccount): void {
    this.methodCalled('setAsActiveAccount', account);
  }
}
