// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The mock signin.ProfileBrowserProxy.
 * @implements {signin.ProfileBrowserProxy}
 */
class TestProfileBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAvailableIcons',
      'launchGuestUser',
      'createProfile',
      'initializeUserManager',
      'launchUser',
      'areAllProfilesLocked',
    ]);

    /** @private {!Array<!AvatarIcon>} */
    this.icons_ = [];

    /** @private {boolean} */
    this.allProfilesLocked_ = false;
  }

  /**
   * @param {!Array<!AvatarIcon>} icons
   */
  setIcons(icons) {
    this.icons_ = icons;
  }

  /**
   * @param {boolean} allProfilesLocked
   */
  setAllProfilesLocked(allProfilesLocked) {
    this.allProfilesLocked_ = allProfilesLocked;
  }

  /** @override */
  getAvailableIcons() {
    this.methodCalled('getAvailableIcons');
    cr.webUIListenerCallback('profile-icons-received', this.icons_);
  }

  /** @override */
  createProfile(profileName, profileIconUrl, createShortcut) {
    this.methodCalled('createProfile', {
      profileName: profileName,
      profileIconUrl: profileIconUrl,
      createShortcut: createShortcut
    });
  }

  /** @override */
  launchGuestUser() {
    this.methodCalled('launchGuestUser');
  }

  /** @override */
  areAllProfilesLocked() {
    this.methodCalled('areAllProfilesLocked');
    return Promise.resolve(this.allProfilesLocked_);
  }
}
