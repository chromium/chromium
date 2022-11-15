// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {getNavigationViewForPageId} from 'chrome://diagnostics/diagnostics_utils.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/chromeos/test_browser_proxy.js';

/** Test version of DiagnosticsBrowserProxy. */
export class TestDiagnosticsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initialize',
      'recordNavigation',
      'saveSessionLog',
      'getPluralString',
    ]);

    /** @private {boolean} */
    this.success_ = false;

    /** @private {?NavigationView} */
    this.previousView_ = null;
  }

  /** @override */
  initialize() {
    this.methodCalled('initialize');
  }

  /** @override */
  recordNavigation(currentView) {
    this.methodCalled(
        'recordNavigation',
        [this.previousView_, getNavigationViewForPageId(currentView)]);
  }

  /**
   * @return {!Promise}
   * @override
   */
  saveSessionLog() {
    this.methodCalled('saveSessionLog');
    return Promise.resolve(this.success_);
  }

  /** @param {NavigationView} view */
  setPreviousView(view) {
    this.previousView_ = view;
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
