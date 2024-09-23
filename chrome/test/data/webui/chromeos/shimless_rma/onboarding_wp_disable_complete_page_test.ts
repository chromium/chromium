// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingWpDisableCompletePage} from 'chrome://shimless-rma/onboarding_wp_disable_complete_page.js';
import {StateResult, WriteProtectDisableCompleteAction} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingWpDisableCompletePageTest', function() {
  let component: OnboardingWpDisableCompletePage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function resetPageWithAction(action: WriteProtectDisableCompleteAction):
      Promise<void> {
    component?.remove();
    component = null;
    return initializeOnboardingWpDisableCompletePage(action);
  }

  function initializeOnboardingWpDisableCompletePage(
      action: WriteProtectDisableCompleteAction =
          WriteProtectDisableCompleteAction.kCompleteAssembleDevice):
      Promise<void> {
    assert(service);
    service.setGetWriteProtectDisableCompleteAction(action);

    assert(!component);
    component = document.createElement(OnboardingWpDisableCompletePage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the page initializes and renders.
  test('PageRenders', async () => {
    await initializeOnboardingWpDisableCompletePage();

    assert(component);
    const basePage =
        strictQuery('base-page', component.shadowRoot, HTMLElement);
    assert(basePage);
  });

  // Verify the correct string shows based on the current action.
  test('SetActionMessage', async () => {
    await initializeOnboardingWpDisableCompletePage(
        WriteProtectDisableCompleteAction.kUnknown);

    const actionSelector = '#writeProtectAction';
    assert(component);
    let actionComponent =
        strictQuery(actionSelector, component.shadowRoot, HTMLElement);
    assertEquals('', actionComponent.textContent!.trim());

    await resetPageWithAction(
        WriteProtectDisableCompleteAction.kSkippedAssembleDevice);
    actionComponent =
        strictQuery(actionSelector, component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('wpDisableReassembleNowText'),
        actionComponent.textContent!.trim());

    await resetPageWithAction(
        WriteProtectDisableCompleteAction.kCompleteAssembleDevice);
    actionComponent =
        strictQuery(actionSelector, component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('wpDisableReassembleNowText'),
        actionComponent.textContent!.trim());

    await resetPageWithAction(
        WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen);
    actionComponent =
        strictQuery(actionSelector, component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('wpDisableLeaveDisassembledText'),
        actionComponent.textContent!.trim());

    await resetPageWithAction(WriteProtectDisableCompleteAction.kCompleteNoOp);
    actionComponent =
        strictQuery(actionSelector, component.shadowRoot, HTMLElement);
    assertEquals('', actionComponent.textContent!.trim());
  });

  // Verify clicking the next button confirms the wp disable is complete.
  test('NextCallsConfirmManualWpDisableComplete', async () => {
    await initializeOnboardingWpDisableCompletePage();

    const expectedPromise = new PromiseResolver<{stateResult: StateResult}>();
    assert(service);
    service.confirmManualWpDisableComplete = () => expectedPromise.promise;

    assert(component);
    assertEquals(expectedPromise.promise, component.onNextButtonClick());
  });
});
