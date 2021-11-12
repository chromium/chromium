// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {FinalizationStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
import {WrapupFinalizePage} from 'chrome://shimless-rma/wrapup_finalize_page.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function wrapupFinalizePageTest() {
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
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeFinalizePage() {
    assertFalse(!!component);

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
}
