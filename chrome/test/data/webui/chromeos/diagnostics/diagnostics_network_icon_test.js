// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiagnosticsNetworkIconElement} from 'chrome://diagnostics/diagnostics_network_icon.js';
import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function diagnosticsNetworkIconTestSuite() {
  /** @type {?DiagnosticsNetworkIconElement} */
  let diagnosticsNetworkIconElement = null;


  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    diagnosticsNetworkIconElement.remove();
    diagnosticsNetworkIconElement = null;
  });

  /** @return {!Promise} */
  function initializeDiagnosticsNetworkIcon() {
    assertFalse(!!diagnosticsNetworkIconElement);

    diagnosticsNetworkIconElement =
        document.createElement('diagnostics-network-icon');
    assertTrue(!!diagnosticsNetworkIconElement);
    document.body.appendChild(diagnosticsNetworkIconElement);

    return flushTasks();
  }

  test('DiagnosticsNetworkIcon', () => {
    return initializeDiagnosticsNetworkIcon().then(
        () => assertTrue(!!diagnosticsNetworkIconElement));
  });
}
