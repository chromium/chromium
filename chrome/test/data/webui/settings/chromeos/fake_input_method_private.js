// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.inputMethodPrivate
 * for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.inputMethodsPrivate API. Only methods that are called
   * during testing have been implemented.
   *
   * @constructor
   * @implements {InputMethodPrivate}
   */
  function FakeInputMethodPrivate() {}

  FakeInputMethodPrivate.prototype = {
    getCurrentInputMethod: function(callback) {
      callback(null);
    },

    get onChanged() {
      return {
        addListener: function() {
          // Nothing to do here.
        },
        removeListener: function() {
          // Nothing to do here.
        },
      };
    },
  };

  return {FakeInputMethodPrivate: FakeInputMethodPrivate};
});
