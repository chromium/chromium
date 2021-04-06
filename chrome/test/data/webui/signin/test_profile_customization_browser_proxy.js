// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProfileCustomizationBrowserProxy, ProfileInfo} from 'chrome://profile-customization/profile_customization_browser_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {ProfileCustomizationBrowserProxy} */
export class TestProfileCustomizationBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['done', 'initialized']);
    /** @private {!ProfileInfo} */
    this.profileInfo_ = {
      backgroundColor: '',
      pictureUrl: '',
      isManaged: false,
      welcomeTitle: '',
    };
  }

  /** @param {!ProfileInfo} info */
  setProfileInfo(info) {
    this.profileInfo_ = info;
  }

  /** @override */
  initialized() {
    this.methodCalled('initialized');
    return Promise.resolve(this.profileInfo_);
  }

  /** @override */
  done(profileName) {
    this.methodCalled('done', profileName);
  }
}
