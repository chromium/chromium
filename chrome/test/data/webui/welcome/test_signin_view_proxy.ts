// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {SigninViewProxy} from 'chrome://welcome/signin_view_proxy.js';

export class TestSigninViewProxy extends TestBrowserProxy implements
    SigninViewProxy {
  constructor() {
    super([
      'recordPageShown',
      'recordNavigatedAway',
      'recordNavigatedAwayThroughBrowserHistory',
      'recordSkip',
      'recordSignIn',
    ]);
  }

  recordPageShown() {
    this.methodCalled('recordPageShown');
  }

  recordNavigatedAway() {
    this.methodCalled('recordNavigatedAway');
  }

  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }

  recordSkip() {
    this.methodCalled('recordSkip');
  }

  recordSignIn() {
    this.methodCalled('recordSignIn');
  }
}
