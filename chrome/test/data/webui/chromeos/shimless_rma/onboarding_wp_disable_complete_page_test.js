// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingWpDisableCompletePage} from 'chrome://shimless-rma/onboarding_wp_disable_complete_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {WriteProtectDisableCompleteAction} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingWpDisableCompletePageTest', function() {
  /** @type {?OnboardingWpDisableCompletePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
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
});
