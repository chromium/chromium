// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {GoogleAppProxy} from 'chrome://welcome/google_apps/google_app_proxy.js';
import type {BookmarkListItem} from 'chrome://welcome/shared/nux_types.js';

export class TestGoogleAppProxy extends TestBrowserProxy implements
    GoogleAppProxy {
  providerSelectedCount: number = 0;
  private appList_: BookmarkListItem[] = [];

  constructor() {
    super([
      'cacheBookmarkIcon',
      'getAppList',
      'recordProviderSelected',
    ]);
  }

  getAppList() {
    this.methodCalled('getAppList');
    return Promise.resolve(this.appList_);
  }

  cacheBookmarkIcon(appId: number) {
    this.methodCalled('cacheBookmarkIcon', appId);
  }

  recordProviderSelected(providerId: number) {
    this.methodCalled('recordProviderSelected', providerId);
    this.providerSelectedCount++;
  }

  setAppList(appList: BookmarkListItem[]) {
    this.appList_ = appList;
  }
}
