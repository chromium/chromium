// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {LandingViewProxy} from 'chrome://welcome/landing_view_proxy.js';

export class TestLandingViewProxy extends TestBrowserProxy implements
    LandingViewProxy {
  constructor() {
    super([
      'recordPageShown',
      'recordNavigatedAway',
      'recordNewUser',
      'recordExistingUser',
    ]);
  }

  recordPageShown() {
    this.methodCalled('recordPageShown');
  }

  recordNavigatedAway() {
    this.methodCalled('recordNavigatedAway');
  }

  recordNewUser() {
    this.methodCalled('recordNewUser');
  }

  recordExistingUser() {
    this.methodCalled('recordExistingUser');
  }
}
