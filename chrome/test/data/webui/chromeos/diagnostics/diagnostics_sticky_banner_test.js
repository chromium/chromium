// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiagnosticsStickyBannerElement} from 'chrome://diagnostics/diagnostics_sticky_banner.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function diagnosticsStickyBannerTestSuite() {
  /** @type {?DiagnosticsStickyBannerElement} */
  let diagnosticsStickyBannerElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    diagnosticsStickyBannerElement.remove();
    diagnosticsStickyBannerElement = null;
  });

  /** @return {!Promise} */
  function initializeDiagnosticsStickyBanner() {
    assertFalse(!!diagnosticsStickyBannerElement);

    // Add the sticky banner to the DOM.
    diagnosticsStickyBannerElement =
        /** @type {!DiagnosticsStickyBannerElement} */ (
            document.createElement('diagnostics-sticky-banner'));
    assertTrue(!!diagnosticsStickyBannerElement);
    document.body.appendChild(diagnosticsStickyBannerElement);

    return flushTasks();
  }

  /** @return {!Element} */
  function getBanner() {
    assertTrue(!!diagnosticsStickyBannerElement);

    return /** @type {!Element} */ (
        diagnosticsStickyBannerElement.shadowRoot.querySelector('#banner'));
  }

  /** @return {!Element} */
  function getBannerMsg() {
    assertTrue(!!diagnosticsStickyBannerElement);

    return /** @type {!Element} */ (
        diagnosticsStickyBannerElement.shadowRoot.querySelector('#bannerMsg'));
  }

  /**
   * @param {string} message
   * @return {!Promise}
   */
  function setBannerMessage(message) {
    assertTrue(!!diagnosticsStickyBannerElement);
    diagnosticsStickyBannerElement.bannerMessage = message;

    return flushTasks();
  }

  test('BannerInitializedCorrectly', () => {
    return initializeDiagnosticsStickyBanner().then(() => {
      assertFalse(isVisible(getBanner()));
    });
  });

  test('BannerShowsWhenMessageSetToNonEmptyString', () => {
    const testMessage = 'Infomational banner';
    return initializeDiagnosticsStickyBanner()
        .then(() => setBannerMessage(testMessage))
        .then(() => {
          assertTrue(isVisible(getBanner()));
          dx_utils.assertElementContainsText(getBannerMsg(), testMessage);
        })
        .then(() => setBannerMessage(''))
        .then(() => {
          assertFalse(isVisible(getBanner()));
          dx_utils.assertElementDoesNotContainText(getBannerMsg(), testMessage);
        });
  });
}
