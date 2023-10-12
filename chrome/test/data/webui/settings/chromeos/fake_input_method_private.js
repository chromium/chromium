// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.inputMethodPrivate
 * for testing.
 */
/**
 * Fake of the chrome.inputMethodsPrivate API. Only methods that are called
 * during testing have been implemented.
 *
 * @constructor
 */
export function FakeInputMethodPrivate() {}

FakeInputMethodPrivate.prototype = {
  getCurrentInputMethod: () => Promise.resolve(null),

  setCurrentInputMethod: () => Promise.resolve(),

  getLanguagePackStatus: () =>
      Promise.resolve(chrome.inputMethodPrivate.LanguagePackStatus.UNKNOWN),

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
