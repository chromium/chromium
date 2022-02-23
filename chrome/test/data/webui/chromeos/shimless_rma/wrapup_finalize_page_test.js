// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {FinalizationStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
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

  test('FinalizationIncompleteDisablesNext', async () => {
    await initializeFinalizePage();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Finalization is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('FinalizationCompleteEnablesNext', async () => {
    const resolver = new PromiseResolver();
    await initializeFinalizePage();
    service.triggerFinalizationObserver(FinalizationStatus.kComplete, 1.0, 0);
    await flushTasks();
    service.finalizationComplete = () => {
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertDeepEquals(savedResult, expectedResult);
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
        FinalizationStatus.kFailedBlocking, 1.0, 0);
    await flushTasks();

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
        FinalizationStatus.kFailedNonBlocking, 1.0, 0);
    await flushTasks();

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
}
