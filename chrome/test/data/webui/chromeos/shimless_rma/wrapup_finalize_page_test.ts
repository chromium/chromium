// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FATAL_HARDWARE_ERROR} from 'chrome://shimless-rma/events.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {FinalizationError, FinalizationStatus, RmadErrorCode, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {FINALIZATION_ERROR_CODE_PREFIX, WrapupFinalizePage} from 'chrome://shimless-rma/wrapup_finalize_page.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('wrapupFinalizePageTest', function() {
  // ShimlessRma is needed to handle the 'transition-state' event used when
  // handling the finalize actions.
  let shimlessRmaComponent: ShimlessRma|null = null;

  let component: WrapupFinalizePage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  function initializeFinalizePage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(WrapupFinalizePage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify components displays as expected.
  test('FinalizePageInitializes', async () => {
    await initializeFinalizePage();

    assert(component);
    assertFalse(
        strictQuery('#finalizationMessage', component.shadowRoot, HTMLElement)
            .hidden);
  });

  // Verify the page auto transitions when finalization completes.
  test('FinalizationCompleteAutoTransitions', async () => {
    await initializeFinalizePage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    service.finalizationComplete = () => {
      ++callCount;
      return resolver.promise;
    };
    service.triggerFinalizationObserver(
        FinalizationStatus.kComplete, /* progress= */ 1.0,
        FinalizationError.kUnknown, /* delayMs= */ 0);
    await flushTasks();

    assertEquals(1, callCount);
  });

  // Verify all finalization errors trigger the expected hardware error.
  test('AllErrorsTriggerFatalHardwareErrorEvent', async () => {
    await initializeFinalizePage();

    assert(component);
    let errorEvent = eventToPromise(FATAL_HARDWARE_ERROR, component);
    let expectedFinalizationError = FinalizationError.kCannotEnableHardwareWp;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, /* progress= */ 0.0,
        expectedFinalizationError, /* delayMs= */ 0);
    let errorEventResult = await errorEvent;
    assertEquals(
        RmadErrorCode.kFinalizationFailed,
        errorEventResult.detail.rmadErrorCode);
    assertEquals(
        FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
        errorEventResult.detail.fatalErrorCode);

    errorEvent = eventToPromise(FATAL_HARDWARE_ERROR, component);
    expectedFinalizationError = FinalizationError.kCannotEnableSoftwareWp;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, /* progress= */ 0.0,
        expectedFinalizationError, /* delayMs= */ 0);
    errorEventResult = await errorEvent;
    assertEquals(
        RmadErrorCode.kFinalizationFailed,
        errorEventResult.detail.rmadErrorCode);
    assertEquals(
        FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
        errorEventResult.detail.fatalErrorCode);

    errorEvent = eventToPromise(FATAL_HARDWARE_ERROR, component);
    expectedFinalizationError = FinalizationError.kCr50;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, /* progress= */ 0.0,
        expectedFinalizationError, /* delayMs= */ 0);
    errorEventResult = await errorEvent;
    assertEquals(
        RmadErrorCode.kFinalizationFailed,
        errorEventResult.detail.rmadErrorCode);
    assertEquals(
        FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
        errorEventResult.detail.fatalErrorCode);

    errorEvent = eventToPromise(FATAL_HARDWARE_ERROR, component);
    expectedFinalizationError = FinalizationError.kGbb;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, /* progress= */ 0.0,
        expectedFinalizationError, /* delayMs= */ 0);
    errorEventResult = await errorEvent;
    assertEquals(
        RmadErrorCode.kFinalizationFailed,
        errorEventResult.detail.rmadErrorCode);
    assertEquals(
        FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
        errorEventResult.detail.fatalErrorCode);

    errorEvent = eventToPromise(FATAL_HARDWARE_ERROR, component);
    expectedFinalizationError = FinalizationError.kUnknown;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, /* progress= */ 0.0,
        expectedFinalizationError, /* delayMs= */ 0);
    errorEventResult = await errorEvent;
    assertEquals(
        RmadErrorCode.kFinalizationFailed,
        errorEventResult.detail.rmadErrorCode);
    assertEquals(
        FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
        errorEventResult.detail.fatalErrorCode);

    errorEvent = eventToPromise(FATAL_HARDWARE_ERROR, component);
    expectedFinalizationError = FinalizationError.kInternal;
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, /* progress= */ 0.0,
        expectedFinalizationError, /* delayMs= */ 0);
    errorEventResult = await errorEvent;
    assertEquals(
        RmadErrorCode.kFinalizationFailed,
        errorEventResult.detail.rmadErrorCode);
    assertEquals(
        FINALIZATION_ERROR_CODE_PREFIX + expectedFinalizationError,
        errorEventResult.detail.fatalErrorCode);
  });
});
