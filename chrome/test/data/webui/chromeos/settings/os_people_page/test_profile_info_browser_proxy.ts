// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProfileInfo, ProfileInfoBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestProfileInfoBrowserProxy extends TestBrowserProxy implements
    ProfileInfoBrowserProxy {
  fakeProfileInfo: ProfileInfo;

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

  getProfileInfo(): Promise<ProfileInfo> {
    this.methodCalled('getProfileInfo');
    return Promise.resolve(this.fakeProfileInfo);
  }

  getProfileStatsCount(): void {
    this.methodCalled('getProfileStatsCount');
  }
}
