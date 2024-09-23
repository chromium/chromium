// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {CLICK_EXIT_BUTTON, TRANSITION_STATE} from 'chrome://shimless-rma/events.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('onboardingLandingPageTest', function() {
  let component: OnboardingLandingPage|null = null;

  let service: FakeShimlessRmaService|null = null;

  const busyIconSelector = '#busyIcon';
  const verificationIconSelector = '#verificationIcon';
  const unqualifiedComponentsLinkSelector = '#unqualifiedComponentsLink';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    service = null;
  });

  function initializeLandingPage(): Promise<void> {
    assert(!component);
    component = document.createElement(OnboardingLandingPage.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  // Verify the component is rendered.
  test('ComponentRenders', async () => {
    await initializeLandingPage();
    assert(component);
  });

  // Verify going to the next page fails when hardware verification is not yet
  // complete.
  test('VerificationNotCompleteNextFails', async () => {
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    await initializeLandingPage();
    let callCounter = 0;
    assert(service);
    service.beginFinalization = () => {
      ++callCounter;
      return resolver.promise;
    };

    let savedError;
    try {
      assert(component);
      await component.onNextButtonClick();
    } catch (error: unknown) {
      assertTrue(error instanceof Error);
      savedError = error as Error;
    }

    const expectedCallCount = 0;
    assertEquals(expectedCallCount, callCounter);
    assert(savedError);
    assertEquals('Hardware verification is not complete.', savedError.message);
  });

  // Verify clicking the next button begins finalization.
  test('ValidationCompleteOnNextCallsBeginFinalization', async () => {
    await initializeLandingPage();

    assert(service);
    service.triggerHardwareVerificationStatusObserver(
        /* isCompliant= */ true, /* errorMessage= */ '', /* delayMs= */ 0);
    await flushTasks();

    const expectedPromise = new PromiseResolver<{stateResult: StateResult}>();
    service.beginFinalization = () => expectedPromise.promise;

    assert(component);
    assertEquals(expectedPromise.promise, component.onNextButtonClick());
  });

  // Verify after validation, the busy icon is hidden and the verification icon
  // shows.
  test('ValidationSuccessCheckShows', async () => {
    await initializeLandingPage();

    assert(component);
    const busyIcon =
        strictQuery(busyIconSelector, component.shadowRoot, HTMLElement);
    const verification = strictQuery(
                             verificationIconSelector, component.shadowRoot,
                             HTMLElement) as IronIconElement;
    assertTrue(isVisible(busyIcon));
    assertFalse(isVisible(verification));

    assert(service);
    service.triggerHardwareVerificationStatusObserver(
        /* isCompliant= */ true, /* errorMessage= */ '', /* delayMs= */ 0);
    await flushTasks();

    assertFalse(isVisible(busyIcon));
    assertTrue(isVisible(verification));
    assertEquals('shimless-icon:check', verification.icon);
  });

  // Verify the unqualified link shows if validation fails and the components
  // dialog can be opened.
  test('ValidationFailedWarning', async () => {
    await initializeLandingPage();

    const failedComponent = 'Keyboard';
    assert(service);
    service.triggerHardwareVerificationStatusObserver(
        /* isCompliant= */ false, failedComponent, /* delayMs= */ 0);
    await flushTasks();

    assert(component);
    assertTrue(strictQuery(busyIconSelector, component.shadowRoot, HTMLElement)
                   .hidden);
    assertTrue(isVisible(strictQuery(
        verificationIconSelector, component.shadowRoot, HTMLElement)));

    // Open the dialog and verify the contents.
    strictQuery(
        unqualifiedComponentsLinkSelector, component.shadowRoot, HTMLElement)
        .click();
    assertTrue(strictQuery(
                   '#unqualifiedComponentsDialog', component.shadowRoot,
                   CrDialogElement)
                   .open);
    assertEquals(
        failedComponent,
        strictQuery('#dialogBody', component.shadowRoot, HTMLElement)
            .textContent!.trim());
  });

  // Verify clicking the landing page's exit button sends the correct event.
  test('ExitButtonDispatchesExitEvent', async () => {
    await initializeLandingPage();

    assert(component);
    const clickExitButtonEvent = eventToPromise(CLICK_EXIT_BUTTON, component);

    strictQuery('#landingExit', component.shadowRoot, CrButtonElement).click();
    await flushTasks();
    await clickExitButtonEvent;
  });

  // Verify clicking the landing page's get started button sends the correct
  // event.
  test('GetStartedButtonDispatchesTransitionStateEvent', async () => {
    await initializeLandingPage();

    assert(component);
    const transitionStateEvent = eventToPromise(TRANSITION_STATE, component);

    const getStartedButton =
        strictQuery('#getStartedButton', component.shadowRoot, CrButtonElement);
    getStartedButton.disabled = false;
    getStartedButton.click();
    await flushTasks();
    await transitionStateEvent;
  });
});
