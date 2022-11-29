// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {FinalizationError, FinalizationStatus, RmadErrorCode} from 'chrome://shimless-rma/shimless_rma_types.js';
import {FINALIZATION_ERROR_CODE_PREFIX, WrapupFinalizePage} from 'chrome://shimless-rma/wrapup_finalize_page.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('wrapupFinalizePageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?WrapupFinalizePage} */
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
  function initializeFinalizePage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!WrapupFinalizePage} */ (
        document.createElement('wrapup-finalize-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('FinalizePageInitializes', async () => {
    await initializeFinalizePage();
    const manualEnableComponent =
        component.shadowRoot.querySelector('#finalizationMessage');
    assertFalse(manualEnableComponent.hidden);
  });

  test('FinalizationCompleteAutoTransitions', async () => {
    const resolver = new PromiseResolver();
    await initializeFinalizePage();

    let callCount = 0;
    service.finalizationComplete = () => {
      callCount++;
      return resolver.promise;
    };
    service.triggerFinalizationObserver(
        FinalizationStatus.kComplete, 1.0, FinalizationError.kUnknown, 0);
    await flushTasks();

    assertEquals(1, callCount);
  });

  test('AllErrorsTriggerFatalHardwareErrorEvent', async () => {
    await initializeFinalizePage();

    let hardwareErrorEventFired = false;
    let expectedFinalizationError;

    const eventHandler = (event) => {
      hardwareErrorEventFired = true;
      assertEquals(
          RmadErrorCode.kFinalizationFailed, event.detail.rmadErrorCode);
      assertEquals(
          FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
          event.detail.fatalErrorCode);
    };

    component.addEventListener('fatal-hardware-error', eventHandler);

    expectedFinalizationError = FinalizationError.kCannotEnableHardwareWp;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, expectedFinalizationError, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    expectedFinalizationError = FinalizationError.kCannotEnableSoftwareWp;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, expectedFinalizationError, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    expectedFinalizationError = FinalizationError.kCr50;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, expectedFinalizationError, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    expectedFinalizationError = FinalizationError.kGbb;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, expectedFinalizationError, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    expectedFinalizationError = FinalizationError.kUnknown;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, expectedFinalizationError, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    expectedFinalizationError = FinalizationError.kInternal;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, expectedFinalizationError, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    component.removeEventListener('fatal-hardware-error', eventHandler);
  });
});
