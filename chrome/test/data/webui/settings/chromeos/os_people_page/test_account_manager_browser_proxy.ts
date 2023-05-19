// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Account, AccountManagerBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAccountManagerBrowserProxy extends TestBrowserProxy implements
    AccountManagerBrowserProxy {
  constructor() {
    super([
      'getAccounts',
      'addAccount',
      'reauthenticateAccount',
      'removeAccount',
      'migrateAccount',
      'changeArcAvailability',
    ]);
  }

  getAccounts(): Promise<Account[]> {
    this.methodCalled('getAccounts');

    return Promise.resolve([
      {
        id: '123',
        accountType: 1,
        isDeviceAccount: true,
        isSignedIn: true,
        unmigrated: false,
        isManaged: true,
        fullName: 'Primary Account',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'primary@gmail.com',
        isAvailableInArc: true,
      },
      {
        id: '456',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: true,
        unmigrated: false,
        isManaged: false,
        fullName: 'Secondary Account 1',
        email: 'user1@example.com',
        pic: '',
        isAvailableInArc: true,
      },
      {
        id: '789',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: false,
        unmigrated: false,
        isManaged: false,
        fullName: 'Secondary Account 2',
        email: 'user2@example.com',
        pic: '',
        isAvailableInArc: false,
      },
    ]);
  }

  addAccount(): void {
    this.methodCalled('addAccount');
  }

  reauthenticateAccount(accountEmail: string): void {
    this.methodCalled('reauthenticateAccount', accountEmail);
  }

  removeAccount(account: Account): void {
    this.methodCalled('removeAccount', account);
  }

  migrateAccount(accountEmail: string): void {
    this.methodCalled('migrateAccount', accountEmail);
  }

  changeArcAvailability(account: Account, isAvailableInArc: boolean): void {
    this.methodCalled('changeArcAvailability', [account, isAvailableInArc]);
  }
}
