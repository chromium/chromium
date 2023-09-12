// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {Shimless3pDiagnostics} from 'chrome://shimless-rma/shimless_3p_diagnostics.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise} from '../test_util.js';

suite('shimless3pDiagTest', function() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?Shimless3pDiagnostics} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
    loadTimeData.overrideValues({'3pDiagnosticsEnabled': true});
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
  const initialize = () => {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!Shimless3pDiagnostics} */ (
        shimlessRmaComponent.shadowRoot.querySelector(
            '#shimless3pDiagnostics'));
    assertTrue(!!component);

    return flushTasks();
  };

  // Test initialization of 3p diag.
  test('initialize', async () => {
    await initialize();
    assertTrue(!!component);
  });
});
