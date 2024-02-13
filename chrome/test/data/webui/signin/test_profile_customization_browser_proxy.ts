// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProfileCustomizationBrowserProxy, ProfileInfo} from 'chrome://profile-customization/profile_customization_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestProfileCustomizationBrowserProxy extends TestBrowserProxy
    implements ProfileCustomizationBrowserProxy {
  private profileInfo_: ProfileInfo;

  constructor() {
    super([
      'done',
      'initialized',
      'getAvailableIcons',
      'skip',
      'deleteProfile',
      'setAvatarIcon',
    ]);

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

  getAvailableIcons() {
    this.methodCalled('getAvailableIcons');
    return Promise.resolve([
      {
        url: 'fake-icon-0.png',
        label: 'fake-icon-0',
        index: 0,
        selected: true,
        isGaiaAvatar: false,
      },
      {
        url: 'fake-icon-1.png',
        label: 'fake-icon-1',
        index: 1,
        selected: false,
        isGaiaAvatar: false,
      },
      {
        url: 'fake-icon-2.png',
        label: 'fake-icon-2',
        index: 2,
        selected: false,
        isGaiaAvatar: false,
      },
    ]);
  }

  done(profileName: string) {
    this.methodCalled('done', profileName);
  }

  skip() {
    this.methodCalled('skip');
  }

  deleteProfile() {
    this.methodCalled('deleteProfile');
  }

  setAvatarIcon(avatarIndex: number) {
    this.methodCalled('setAvatarIcon', avatarIndex);
  }
}
