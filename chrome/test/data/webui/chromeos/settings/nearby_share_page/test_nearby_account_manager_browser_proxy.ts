// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Account, NearbyAccountManagerBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestNearbyAccountManagerBrowserProxy extends TestBrowserProxy
    implements NearbyAccountManagerBrowserProxy {
  constructor() {
    super([
      'getAccounts',
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
        fullName: 'Primary Account',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'primary@gmail.com',
        organization: undefined,
      },
    ]);
  }
}
