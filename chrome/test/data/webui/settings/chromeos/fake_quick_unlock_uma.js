// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome histogram recording for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.quickUnlockUma.
   * @constructor
   */
  function FakeQuickUnlockUma() {
    this.histogram = {};
    for (const key in LockScreenProgress) {
      this.histogram[LockScreenProgress[key]] = 0;
    }
  }

  FakeQuickUnlockUma.prototype = {
    /**
     * Update the histgoram at |key| by one.
     * @param {LockScreenProgress} key
     */
    recordProgress: function(key) {
      if (!(key in this.histogram)) {
        this.histogram[key] = 0;
      }
      this.histogram[key]++;
    },

    /**
     * Get the value of the uma histogram at |key|.
     * @param {LockScreenProgress} key
     * @return {Number}
     */
    getHistogramValue: function(key) {
      return this.histogram[key];
    }
  };

  return {FakeQuickUnlockUma: FakeQuickUnlockUma};
});
