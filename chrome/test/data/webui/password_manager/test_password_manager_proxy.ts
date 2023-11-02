// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

import {PasswordCheckInteraction, PasswordCheckStatusChangedListener, PasswordManagerProxy, SavedPasswordListChangedListener} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {makePasswordCheckStatus} from './test_util.js';

/**
 * Test implementation
 */
export class TestPasswordManagerProxy extends TestBrowserProxy implements
    PasswordManagerProxy {
  data: {
    passwords: chrome.passwordsPrivate.PasswordUiEntry[],
    checkStatus: chrome.passwordsPrivate.PasswordCheckStatus,
  };

  listeners: {
    savedPasswordListChangedListener: SavedPasswordListChangedListener|null,
    passwordCheckStatusListener: PasswordCheckStatusChangedListener|null,
  };

  constructor() {
    super([
      'getPasswordCheckStatus',
      'getSavedPasswordList',
      'recordPasswordCheckInteraction',
      'startBulkPasswordCheck',
    ]);

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
      checkStatus: makePasswordCheckStatus(),
    };

    // Holds listeners so they can be called when needed.
    this.listeners = {
      passwordCheckStatusListener: null,
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

  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener) {
    this.listeners.passwordCheckStatusListener = listener;
  }

  removePasswordCheckStatusListener(_listener:
                                        PasswordCheckStatusChangedListener) {
    this.listeners.passwordCheckStatusListener = null;
  }

  getSavedPasswordList(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]> {
    this.methodCalled('getSavedPasswordList');
    return Promise.resolve(this.data.passwords);
  }

  getPasswordCheckStatus() {
    this.methodCalled('getPasswordCheckStatus');
    return Promise.resolve(this.data.checkStatus);
  }

  startBulkPasswordCheck() {
    this.methodCalled('startBulkPasswordCheck');
    if (this.data.checkStatus.state ===
        chrome.passwordsPrivate.PasswordCheckState.NO_PASSWORDS) {
      return Promise.reject(new Error('error'));
    }
    return Promise.resolve();
  }

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    this.methodCalled('recordPasswordCheckInteraction', interaction);
  }
}
