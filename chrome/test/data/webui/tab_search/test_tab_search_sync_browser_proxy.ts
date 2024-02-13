// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabSearchSyncBrowserProxy} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchSyncBrowserProxy extends TestBrowserProxy implements
    TabSearchSyncBrowserProxy {
  constructor() {
    super([
      'getSignInState',
    ]);
  }

  getSignInState() {
    this.methodCalled('getSignInState');
    return Promise.resolve(true);
  }
}
