// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {WelcomeBrowserProxy} from 'chrome://welcome/welcome_browser_proxy.js';

export class TestWelcomeBrowserProxy extends TestBrowserProxy implements
    WelcomeBrowserProxy {
  constructor() {
    super([
      'handleActivateSignIn',
      'handleUserDecline',
      'goToNewTabPage',
      'goToUrl',
    ]);
  }

  handleActivateSignIn(redirectUrl: string|null) {
    this.methodCalled('handleActivateSignIn', redirectUrl);
  }

  handleUserDecline() {
    this.methodCalled('handleUserDecline');
  }

  goToNewTabPage() {
    this.methodCalled('goToNewTabPage');
  }

  goToUrl(url: string) {
    this.methodCalled('goToUrl', url);
  }
}
