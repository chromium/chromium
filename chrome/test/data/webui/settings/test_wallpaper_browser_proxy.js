// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.WallpaperBrowserProxy} */
class TestWallpaperBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isWallpaperSettingVisible',
      'isWallpaperPolicyControlled',
      'openWallpaperManager',
    ]);

    /** @private */
    this.isWallpaperSettingVisible_ = true;

    /** @private */
    this.isWallpaperPolicyControlled_ = false;
  }

  /** @override */
  isWallpaperSettingVisible() {
    this.methodCalled('isWallpaperSettingVisible');
    return Promise.resolve(true);
  }

  /** @override */
  isWallpaperPolicyControlled() {
    this.methodCalled('isWallpaperPolicyControlled');
    return Promise.resolve(this.isWallpaperPolicyControlled_);
  }

  /** @override */
  openWallpaperManager() {
    this.methodCalled('openWallpaperManager');
  }

  /** @param {boolean} Whether the wallpaper is policy controlled. */
  setIsWallpaperPolicyControlled(isPolicyControlled) {
    this.isWallpaperPolicyControlled_ = isPolicyControlled;
  }
}
