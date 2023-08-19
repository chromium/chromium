// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiagnosticPage} from 'chrome://shimless-rma/diagnostic_page.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('diagnosticPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event used by the
   * shutdown buttons.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?DiagnosticPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeDiagnosticPage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!DiagnosticPage} */ (
        document.createElement('diagnostic-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the Diagnostic page can be loaded.
  test('InitializeDiagnosticPage', async () => {
    await initializeDiagnosticPage();
    assertTrue(!!component);
  });
});
