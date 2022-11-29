// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {PROVISIONING_ERROR_CODE_PREFIX, ReimagingProvisioningPage} from 'chrome://shimless-rma/reimaging_provisioning_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ProvisioningError, ProvisioningStatus, RmadErrorCode} from 'chrome://shimless-rma/shimless_rma_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('reimagingProvisioningPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?ReimagingProvisioningPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
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
  function initializeWaitForProvisioningPage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

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
        /* error= */ ProvisioningError.kUnknown,
        /* delayMs= */ 0);
    await flushTasks();

    assertTrue(provisioningComplete);
  });

  test('ProvisioningFailedWpError', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForProvisioningPage();

    const wpEnabledDialog =
        component.shadowRoot.querySelector('#wpEnabledDialog');
    assertTrue(!!wpEnabledDialog);
    assertFalse(wpEnabledDialog.open);

    let callCount = 0;
    service.retryProvisioning = () => {
      callCount++;
      return resolver.promise;
    };

    service.triggerProvisioningObserver(
        ProvisioningStatus.kFailedBlocking, 1.0, ProvisioningError.kWpEnabled,
        0);
    await flushTasks();

    assertTrue(wpEnabledDialog.open);

    const tryAgainButton =
        component.shadowRoot.querySelector('#tryAgainButton');
    tryAgainButton.click();

    assertFalse(wpEnabledDialog.open);
    assertEquals(1, callCount);
  });

  test('ProvisioningFailedNonWpError', async () => {
    await initializeWaitForProvisioningPage();

    let hardwareErrorEventFired = false;
    const expectedProvisoningError = ProvisioningError.kInternal;

    const eventHandler = (event) => {
      hardwareErrorEventFired = true;
      assertEquals(
          RmadErrorCode.kProvisioningFailed, event.detail.rmadErrorCode);
      assertEquals(
          PROVISIONING_ERROR_CODE_PREFIX + expectedProvisoningError,
          event.detail.fatalErrorCode);
    };
    component.addEventListener('fatal-hardware-error', eventHandler);

    const wpEnabledDialog =
        component.shadowRoot.querySelector('#wpEnabledDialog');
    assertTrue(!!wpEnabledDialog);
    assertFalse(wpEnabledDialog.open);

    service.triggerProvisioningObserver(
        ProvisioningStatus.kFailedBlocking, 1.0, expectedProvisoningError, 0);
    await flushTasks();

    assertFalse(wpEnabledDialog.open);
    assertTrue(hardwareErrorEventFired);

    component.removeEventListener('fatal-hardware-error', eventHandler);
  });
});
