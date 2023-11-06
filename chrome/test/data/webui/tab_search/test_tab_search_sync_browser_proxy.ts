// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccountInfo, SyncInfo, TabSearchSyncBrowserProxy} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchSyncBrowserProxy extends TestBrowserProxy implements
    TabSearchSyncBrowserProxy {
  accountInfo: AccountInfo;
  syncInfo: SyncInfo;

  constructor() {
    super([
      'getSyncInfo',
      'getAccountInfo',
    ]);

    this.accountInfo = {
      name: 'Jane Doe',
      email: 'testemail@gmail.com',
    };
    this.syncInfo = {
      syncing: true,
      syncingHistory: true,
      paused: false,
    };
  }

  getSyncInfo() {
    this.methodCalled('getSyncInfo');
    return Promise.resolve(this.syncInfo);
  }

  getAccountInfo() {
    this.methodCalled('getAccountInfo');
    return Promise.resolve(this.accountInfo);
  }
}
