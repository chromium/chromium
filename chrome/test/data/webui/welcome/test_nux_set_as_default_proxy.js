// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {NuxSetAsDefaultProxy} */
export class TestNuxSetAsDefaultProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestDefaultBrowserState',
      'setAsDefault',
      'recordPageShown',
      'recordNavigatedAway',
      'recordSkip',
      'recordBeginSetDefault',
      'recordSuccessfullySetDefault',
      'recordNavigatedAwayThroughBrowserHistory',
    ]);

    this.defaultStatus_ = {};
  }

  /** @override */
  requestDefaultBrowserState() {
    this.methodCalled('requestDefaultBrowserState');
    return Promise.resolve(this.defaultStatus_);
  }

  /** @override */
  setAsDefault() {
    this.methodCalled('setAsDefault');
  }

  /** @param {!DefaultBrowserInfo} status */
  setDefaultStatus(status) {
    this.defaultStatus_ = status;
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
  recordSkip() {
    this.methodCalled('recordSkip');
  }

  /** @override */
  recordBeginSetDefault() {
    this.methodCalled('recordBeginSetDefault');
  }

  /** @override */
  recordSuccessfullySetDefault() {
    this.methodCalled('recordSuccessfullySetDefault');
  }

  /** @override */
  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }
}
