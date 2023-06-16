// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {WrapupWaitForManualWpEnablePage} from 'chrome://shimless-rma/wrapup_wait_for_manual_wp_enable_page.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('wrapupWaitForManualWpEnablePageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' used to signal write
   * write protect is enabled.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?WrapupWaitForManualWpEnablePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeWaitForManualWpEnablePage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

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
});
