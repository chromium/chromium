// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {WrapupWaitForManualWpEnablePage} from 'chrome://shimless-rma/wrapup_wait_for_manual_wp_enable_page.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function wrapupWaitForManualWpEnablePageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' used to signal write
   * write protect is enabled.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?WrapupWaitForManualWpEnablePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
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
  function initializeWaitForManualWpEnablePage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!WrapupWaitForManualWpEnablePage} */ (
        document.createElement('wrapup-wait-for-manual-wp-enable-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('WaitForManualWpEnablePageInitializes', async () => {
    await initializeWaitForManualWpEnablePage();
    const manualEnableComponent =
        component.shadowRoot.querySelector('#manuallyEnableHwwpInstructions');
    assertFalse(manualEnableComponent.hidden);
  });

  test('WriteProtectEnabledAutoTransitions', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForManualWpEnablePage();

    let callCount = 0;
    service.writeProtectManuallyEnabled = () => {
      callCount++;
      return resolver.promise;
    };
    service.triggerHardwareWriteProtectionObserver(true, 0);
    await flushTasks();

    assertEquals(1, callCount);
  });
}
