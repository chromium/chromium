// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {KerberosErrorType, KerberosConfigErrorCode} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

// List of fake accounts.
/* #export */ const TEST_KERBEROS_ACCOUNTS = [
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
  }
];

/** @implements {settings.KerberosAccountsBrowserProxy} */
/* #export */ class TestKerberosAccountsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccounts',
      'addAccount',
      'removeAccount',
      'validateConfig',
      'setAsActiveAccount',
    ]);

    // Simulated error from an addAccount call.
    this.addAccountError = settings.KerberosErrorType.kNone;

    // Simulated error from a validateConfig call.
    this.validateConfigResult = {
      error: settings.KerberosErrorType.kNone,
      errorInfo: {code: settings.KerberosConfigErrorCode.kNone}
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
    return Promise.resolve(settings.KerberosErrorType.kNone);
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
