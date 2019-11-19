// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

/** @implements {settings.ProfileInfoBrowserProxy} */
export class TestSyncConfirmationBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'confirm',
      'undo',
      'goToSettings',
      'initializedWithSize',
      'requestAccountImage',
    ]);
  }

  /** @override */
  confirm(description, confirmation) {
    this.methodCalled('confirm', [description, confirmation]);
  }

  /** @override */
  undo() {
    this.methodCalled('undo');
  }

  /** @override */
  goToSettings(description, confirmation) {
    this.methodCalled('goToSettings', [description, confirmation]);
  }

  /** @override */
  initializedWithSize(height) {
    this.methodCalled('initializedWithSize', height);
  }

  /** @override */
  requestAccountImage() {
    this.methodCalled('requestAccountImage');
  }
}
