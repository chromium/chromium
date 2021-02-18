// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LockScreenProgress} from 'chrome://resources/cr_components/chromeos/quick_unlock/lock_screen_constants.m.js';
// clang-format on

/**
 * @fileoverview Fake implementation of chrome histogram recording for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.quickUnlockUma.
   * @constructor
   */
  /* #export */ function FakeQuickUnlockUma() {
    this.histogram = {};
    for (const key in settings.LockScreenProgress) {
      this.histogram[settings.LockScreenProgress[key]] = 0;
    }
  }

  FakeQuickUnlockUma.prototype = {
    /**
     * Update the histgoram at |key| by one.
     * @param {settings.LockScreenProgress} key
     */
    recordProgress: function(key) {
      if (!(key in this.histogram)) {
        this.histogram[key] = 0;
      }
      this.histogram[key]++;
    },

    /**
     * Get the value of the uma histogram at |key|.
     * @param {settings.LockScreenProgress} key
     * @return {Number}
     */
    getHistogramValue: function(key) {
      return this.histogram[key];
    }
  };

  // #cr_define_end
  return {FakeQuickUnlockUma: FakeQuickUnlockUma};
});
