// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProfileCustomizationBrowserProxy, ProfileInfo} from 'chrome://profile-customization/profile_customization_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestProfileCustomizationBrowserProxy extends TestBrowserProxy
    implements ProfileCustomizationBrowserProxy {
  private profileInfo_: ProfileInfo;

  constructor() {
    super(['done', 'initialized', 'skip']);

    this.profileInfo_ = {
      backgroundColor: '',
      pictureUrl: '',
      isManaged: false,
      welcomeTitle: '',
    };
  }

  setProfileInfo(info: ProfileInfo) {
    this.profileInfo_ = info;
  }

  initialized() {
    this.methodCalled('initialized');
    return Promise.resolve(this.profileInfo_);
  }

  done(profileName: string) {
    this.methodCalled('done', profileName);
  }

  skip() {
    this.methodCalled('skip');
  }
}
