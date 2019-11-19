// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {SigninViewProxy} */
export class TestSigninViewProxy extends TestBrowserProxy {
  constructor() {
    super([
      'recordPageShown',
      'recordNavigatedAway',
      'recordNavigatedAwayThroughBrowserHistory',
      'recordSkip',
      'recordSignIn',
    ]);
  }

  /** @override */
  recordPageShown() {
    this.methodCalled('recordPageShown');
  }

  /** @override */
  recordNavigatedAway() {
    this.methodCalled('recordNavigatedAway');
  }

  /** @override */
  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }

  /** @override */
  recordSkip() {
    this.methodCalled('recordSkip');
  }

  /** @override */
  recordSignIn() {
    this.methodCalled('recordSignIn');
  }
}
