// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {FinalizationError, FinalizationStatus, RmadErrorCode} from 'chrome://shimless-rma/shimless_rma_types.js';
import {WrapupFinalizePage} from 'chrome://shimless-rma/wrapup_finalize_page.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function wrapupFinalizePageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

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
    shimless_rma_component.remove();
    shimless_rma_component = null;
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeFinalizePage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

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

    const eventHandler = (event) => {
      hardwareErrorEventFired = true;
      assertEquals(RmadErrorCode.kFinalizationFailed, event.detail);
    };

    component.addEventListener('fatal-hardware-error', eventHandler);

    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0,
        FinalizationError.kCannotEnableHardwareWp, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0,
        FinalizationError.kCannotEnableSoftwareWp, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, FinalizationError.kCr50, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, FinalizationError.kGbb, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, FinalizationError.kUnknown, 0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    hardwareErrorEventFired = false;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 0.0, FinalizationError.kInternal,
        0);
    await flushTasks();
    assertTrue(hardwareErrorEventFired);

    component.removeEventListener('fatal-hardware-error', eventHandler);
  });
}
