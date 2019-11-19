// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {WelcomeBrowserProxy} */
export class TestWelcomeBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'handleActivateSignIn',
      'handleUserDecline',
      'goToNewTabPage',
      'goToURL',
    ]);
  }

  /** @override */
  handleActivateSignIn(providerId) {
    this.methodCalled('handleActivateSignIn', providerId);
  }

  /** @override */
  handleUserDecline(url) {
    this.methodCalled('handleUserDecline', url);
  }

  /** @override */
  goToNewTabPage() {
    this.methodCalled('goToNewTabPage');
  }

  /** @override */
  goToURL(url) {
    this.methodCalled('goToURL', url);
  }
}
