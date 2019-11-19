// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {LandingViewProxy} */
export class TestLandingViewProxy extends TestBrowserProxy {
  constructor() {
    super([
      'recordPageShown',
      'recordNavigatedAway',
      'recordNewUser',
      'recordExistingUser',
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
  recordNewUser() {
    this.methodCalled('recordNewUser');
  }

  /** @override */
  recordExistingUser() {
    this.methodCalled('recordExistingUser');
  }
}
