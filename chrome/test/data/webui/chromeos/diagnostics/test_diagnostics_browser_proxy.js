// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiagnosticsBrowserProxy} from 'chrome://diagnostics/diagnostics_browser_proxy.js';

import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/**
 * Test version of DiagnosticsBrowserProxy.
 * @implements {DiagnosticsBrowserProxy}
 */
export class TestDiagnosticsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initialize',
      'saveSessionLog',
    ]);

    /** @private {boolean} */
    this.success_ = false;
  }

  /** @override */
  initialize() {
    this.methodCalled('initialize');
  }

  /**
   * @return {!Promise}
   * @override
   */
  saveSessionLog() {
    this.methodCalled('saveSessionLog');
    return Promise.resolve(this.success_);
  }

  /** @param {boolean} success */
  setSuccess(success) {
    this.success_ = success;
  }
}