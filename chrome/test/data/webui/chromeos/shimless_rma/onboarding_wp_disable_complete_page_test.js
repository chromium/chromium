// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingWpDisableCompletePage} from 'chrome://shimless-rma/onboarding_wp_disable_complete_page.js';
import {WriteProtectDisableCompleteAction} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';


export function onboardingWpDisableCompletePageTest() {
  /** @type {?OnboardingWpDisableCompletePage} */
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
   * @param {WriteProtectDisableCompleteAction=} action
   * @return {!Promise}
   */
  function initializeOnboardingWpDisableCompletePage(
      action = WriteProtectDisableCompleteAction.kCompleteAssembleDevice) {
    assertFalse(!!component);
    service.setGetWriteProtectDisableCompleteAction(action);

    component = /** @type {!OnboardingWpDisableCompletePage} */ (
        document.createElement('onboarding-wp-disable-complete-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeOnboardingWpDisableCompletePage();
    assertTrue(!!component);

    const basePage = component.shadowRoot.querySelector('base-page');
    assertTrue(!!basePage);
  });

  test('OnBoardingPageSetsActionMessage', async () => {
    await initializeOnboardingWpDisableCompletePage();

    const actionComponent =
        component.shadowRoot.querySelector('#writeProtectAction');

    assertEquals(
        loadTimeData.getString('wpDisableReassembleNowText'),
        actionComponent.textContent.trim());
  });

  test('OnBoardingPageSetsActionKCompleteNoOpMessage', async () => {
    await initializeOnboardingWpDisableCompletePage(
        WriteProtectDisableCompleteAction.kCompleteNoOp);
    const actionComponent =
        component.shadowRoot.querySelector('#writeProtectAction');
    assertEquals('', actionComponent.textContent.trim());
  });

  test('OnBoardingPageOnNextCallsConfirmManualWpDisableComplete', async () => {
    const resolver = new PromiseResolver();
    await initializeOnboardingWpDisableCompletePage();
    let callCounter = 0;
    service.confirmManualWpDisableComplete = () => {
      callCounter++;
      return resolver.promise;
    };

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, callCounter);
    assertDeepEquals(expectedResult, savedResult);
  });
}
