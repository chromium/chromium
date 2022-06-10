// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {FinalizationError, FinalizationStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
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

  test('FinalizationFailedBlockingRetry', async () => {
    const resolver = new PromiseResolver();
    await initializeFinalizePage();

    const retryButton =
        component.shadowRoot.querySelector('#retryFinalizationButton');
    assertTrue(retryButton.hidden);

    let callCount = 0;
    service.retryFinalization = () => {
      callCount++;
      return resolver.promise;
    };
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 1.0, FinalizationError.kInternal,
        0);
    await flushTasks();

    const hardwareWpDisabledDialog =
        component.shadowRoot.querySelector('#hardwareWpDisabledDialog');
    assertTrue(!!hardwareWpDisabledDialog);
    assertFalse(hardwareWpDisabledDialog.open);

    assertFalse(retryButton.hidden);
    retryButton.click();

    await flushTasks();
    assertEquals(1, callCount);
  });

  test('FinalizationFailedNonBlockingRetry', async () => {
    const resolver = new PromiseResolver();
    await initializeFinalizePage();

    const retryButton =
        component.shadowRoot.querySelector('#retryFinalizationButton');
    assertTrue(retryButton.hidden);

    let callCount = 0;
    service.retryFinalization = () => {
      callCount++;
      return resolver.promise;
    };
    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedNonBlocking, 1.0, FinalizationError.kInternal,
        0);
    await flushTasks();

    const hardwareWpDisabledDialog =
        component.shadowRoot.querySelector('#hardwareWpDisabledDialog');
    assertTrue(!!hardwareWpDisabledDialog);
    assertFalse(hardwareWpDisabledDialog.open);

    assertFalse(retryButton.hidden);
    retryButton.click();

    await flushTasks();
    assertEquals(1, callCount);
  });

  test('FinalizationRetryButtonDisabled', async () => {
    await initializeFinalizePage();

    const retryButton =
        component.shadowRoot.querySelector('#retryFinalizationButton');
    assertFalse(retryButton.disabled);
    component.allButtonsDisabled = true;
    assertTrue(retryButton.disabled);
  });

  test('FinalizationFailedHardwareWpError', async () => {
    const resolver = new PromiseResolver();
    await initializeFinalizePage();

    const hardwareWpDisabledDialog =
        component.shadowRoot.querySelector('#hardwareWpDisabledDialog');
    assertTrue(!!hardwareWpDisabledDialog);
    assertFalse(hardwareWpDisabledDialog.open);

    let callCount = 0;
    service.retryFinalization = () => {
      callCount++;
      return resolver.promise;
    };

    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedBlocking, 1.0,
        FinalizationError.kCannotEnableHardwareWp, 0);
    await flushTasks();

    assertTrue(hardwareWpDisabledDialog.open);

    const tryAgainButton =
        component.shadowRoot.querySelector('#tryAgainButton');
    tryAgainButton.click();

    assertFalse(hardwareWpDisabledDialog.open);
    assertEquals(1, callCount);
  });

  test('FinalizationFailedSoftwareWpError', async () => {
    const resolver = new PromiseResolver();
    await initializeFinalizePage();

    const hardwareWpDisabledDialog =
        component.shadowRoot.querySelector('#hardwareWpDisabledDialog');
    assertTrue(!!hardwareWpDisabledDialog);
    assertFalse(hardwareWpDisabledDialog.open);

    let callCount = 0;
    service.retryFinalization = () => {
      callCount++;
      return resolver.promise;
    };

    service.triggerFinalizationObserver(
        FinalizationStatus.kFailedNonBlocking, 1.0,
        FinalizationError.kCannotEnableSoftwareWp, 0);
    await flushTasks();

    assertFalse(hardwareWpDisabledDialog.open);
  });
}
