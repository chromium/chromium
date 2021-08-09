// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiagnosticsBrowserProxy} from 'chrome://diagnostics/diagnostics_browser_proxy.js';

import {assertEquals} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * Test version of DiagnosticsBrowserProxy.
 * @implements {DiagnosticsBrowserProxy}
 */
export class TestDiagnosticsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initialize',
      'saveSessionLog',
      'getPluralString',
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

  /**
   * @param {string} name
   * @param {number} count
   * @return {!Promise}
   */
  getPluralString(name, count) {
    // TODO(michaelcheco): Remove when we have more than one plural string.
    assertEquals(name, 'nameServersText');
    return Promise.resolve(`Name Server${count !== 1 ? 's' : ''}`);
  }
}
