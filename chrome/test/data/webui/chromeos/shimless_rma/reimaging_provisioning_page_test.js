// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingProvisioningPageElement} from 'chrome://shimless-rma/reimaging_provisioning_page.js';
import {ProvisioningStep} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function reimagingProvisioningPageTest() {
  /** @type {?ReimagingProvisioningPageElement} */
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
  function initializeWaitForProvisioningPage() {
    assertFalse(!!component);

    component = /** @type {!ReimagingProvisioningPageElement} */ (
        document.createElement('reimaging-provisioning-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('WaitForManualWpDisablePageInitializes', async () => {
    await initializeWaitForProvisioningPage();
    const provisioningComponent =
        component.shadowRoot.querySelector('#provisioningDeviceStatus');
    assertFalse(provisioningComponent.hidden);
  });

  test('ProvisioningStartingDisablesNext', async () => {
    await initializeWaitForProvisioningPage();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Provisioning is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('ProvisioningInProgressDisablesNext', async () => {
    await initializeWaitForProvisioningPage();
    service.triggerProvisioningObserver(ProvisioningStep.kInProgress, 0.5, 0);
    await flushTasks();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Provisioning is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('ProvisioningEnablesNext', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForProvisioningPage();
    service.triggerProvisioningObserver(
        ProvisioningStep.kProvisioningComplete, 1.0, 0);
    await flushTasks();
    service.transitionNextState = () => {
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
