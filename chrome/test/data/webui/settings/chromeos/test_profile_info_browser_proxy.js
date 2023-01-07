// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {ProfileInfoBrowserProxy} */
export class TestProfileInfoBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getProfileInfo',
      'getProfileStatsCount',
    ]);

    this.fakeProfileInfo = {
      name: 'fakeName',
      iconUrl: 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAE' +
          'AAAICTAEAOw==',
    };
  }

  /** @override */
  getProfileInfo() {
    this.methodCalled('getProfileInfo');
    return Promise.resolve(this.fakeProfileInfo);
  }

  /** @override */
  getProfileStatsCount() {
    this.methodCalled('getProfileStatsCount');
  }
}
