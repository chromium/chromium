// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ManagedUserProfileInfo, ManagedUserProfileNoticeBrowserProxy} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';

export class TestManagedUserProfileNoticeBrowserProxy extends TestBrowserProxy
    implements ManagedUserProfileNoticeBrowserProxy {
  private managedUserProfileInfo_: ManagedUserProfileInfo;
  private mediaQueryList_: FakeMediaQueryList = new FakeMediaQueryList('dummy');

  constructor(info: ManagedUserProfileInfo) {
    super([
      'initialized',
      'initializedWithSize',
      'proceed',
      'cancel',
    ]);

    this.managedUserProfileInfo_ = info;
  }

  setManagedUserProfileInfo(info: ManagedUserProfileInfo) {
    this.managedUserProfileInfo_ = info;
  }

  initialized() {
    this.methodCalled('initialized');
    return Promise.resolve(this.managedUserProfileInfo_);
  }

  initializedWithSize(height: number) {
    this.methodCalled('initializedWithSize', height);
  }

  proceed() {
    this.methodCalled('proceed');
  }

  cancel() {
    this.methodCalled('cancel');
  }

  matchMedia(_query: string): MediaQueryList {
    return this.mediaQueryList_;
  }

  setMatchMediaMatches(matches: boolean): void {
    this.mediaQueryList_.matches = matches;
  }
}
