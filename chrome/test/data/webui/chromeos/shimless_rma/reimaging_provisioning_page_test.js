// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingProvisioningPage} from 'chrome://shimless-rma/reimaging_provisioning_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ProvisioningStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function reimagingProvisioningPageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?ReimagingProvisioningPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  suiteSetup(() => {
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    shimless_rma_component.remove();
    shimless_rma_component = null;
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeWaitForProvisioningPage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!ReimagingProvisioningPage} */ (
        document.createElement('reimaging-provisioning-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ProvisioningCompleteTransitionsState', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForProvisioningPage();

    let provisioningComplete = false;
    service.provisioningComplete = () => {
      provisioningComplete = true;
      return resolver.promise;
    };

    service.triggerProvisioningObserver(
        ProvisioningStatus.kComplete, /* progress= */ 1.0,
        /* delayMs= */ 0);
    await flushTasks();

    assertTrue(provisioningComplete);
  });
}
