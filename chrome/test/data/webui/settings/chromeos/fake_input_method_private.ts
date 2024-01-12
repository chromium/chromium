// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake of the chrome.inputMethodsPrivate API for testing. Only
 * methods that are called during testing have been implemented.
 */

export class FakeInputMethodPrivate {
  getCurrentInputMethod(): Promise<null> {
    return Promise.resolve(null);
  }

  setCurrentInputMethod(): Promise<void> {
    return Promise.resolve();
  }

  getLanguagePackStatus():
      Promise<chrome.inputMethodPrivate.LanguagePackStatus> {
    return Promise.resolve(
        chrome.inputMethodPrivate.LanguagePackStatus.UNKNOWN);
  }

  get onChanged() {
    return {
      addListener: () => {
          // Nothing to do here.
      },
      removeListener: () => {
          // Nothing to do here.
      },
    };
  }
}
