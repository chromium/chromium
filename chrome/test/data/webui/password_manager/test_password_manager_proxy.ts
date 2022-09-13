// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

import {PasswordManagerProxy, SavedPasswordListChangedListener} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test implementation
 */
export class TestPasswordManagerProxy extends TestBrowserProxy implements
    PasswordManagerProxy {
  data: {
    passwords: chrome.passwordsPrivate.PasswordUiEntry[],
  };

  listeners: {
    savedPasswordListChangedListener: SavedPasswordListChangedListener|null,
  };

  constructor() {
    super([
      'getSavedPasswordList',
    ]);

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
    };

    // Holds listeners so they can be called when needed.
    this.listeners = {
      savedPasswordListChangedListener: null,
    };
  }

  addSavedPasswordListChangedListener(listener:
                                          SavedPasswordListChangedListener) {
    this.listeners.savedPasswordListChangedListener = listener;
  }

  removeSavedPasswordListChangedListener(_listener:
                                             SavedPasswordListChangedListener) {
    this.listeners.savedPasswordListChangedListener = null;
  }

  getSavedPasswordList(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]> {
    this.methodCalled('getSavedPasswordList');
    return Promise.resolve(this.data.passwords);
  }
}
