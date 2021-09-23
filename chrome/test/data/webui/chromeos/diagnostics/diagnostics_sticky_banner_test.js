// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiagnosticsStickyBannerElement} from 'chrome://diagnostics/diagnostics_sticky_banner.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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

  test('BannerInitializedCorrectly', () => {
    return initializeDiagnosticsStickyBanner().then(() => {
      assertTrue(!!diagnosticsStickyBannerElement);
    });
  });
}
