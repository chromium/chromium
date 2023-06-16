// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {State} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

suite('onboardingLandingPageTest', function() {
  /** @type {?OnboardingLandingPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeLandingPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingLandingPage} */ (
        document.createElement('onboarding-landing-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeLandingPage();
    assertTrue(!!component);

    const basePage = component.shadowRoot.querySelector('base-page');
    assertTrue(!!basePage);
  });

  test('OnBoardingPageValidationNotCompleteNextDisabled', async () => {
    const resolver = new PromiseResolver();
    await initializeLandingPage();
    let callCounter = 0;
    service.beginFinalization = () => {
      callCounter++;
      return resolver.promise;
    };

    let savedError;
    component.onNextButtonClick().catch((err) => savedError = err);
    await flushTasks();

    assertEquals(0, callCounter);
    assertEquals('Hardware verification is not complete.', savedError.message);
  });

  /**
   * @param {string} buttonNameSelector
   * @return {!Promise}
   */
  function clickButton(buttonNameSelector) {
    assertTrue(!!component);

    const button = component.shadowRoot.querySelector(buttonNameSelector);
    button.click();
    return flushTasks();
  }

  test(
      'OnBoardingPageValidationCompleteOnNextCallsBeginFinalization',
      async () => {
        const resolver = new PromiseResolver();
        await initializeLandingPage();
        service.triggerHardwareVerificationStatusObserver(true, '', 0);
        await flushTasks();
        let callCounter = 0;
        service.beginFinalization = () => {
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

  test('OnBoardingPageValidationSuccessCheckVisible', async () => {
    await initializeLandingPage();

    const busy = component.shadowRoot.querySelector('#busyIcon');
    const verification =
        component.shadowRoot.querySelector('#verificationIcon');
    const error = component.shadowRoot.querySelector('#errorMessage');
    assertTrue(isVisible(busy));
    assertFalse(isVisible(verification));
    assertFalse(isVisible(error));
  });

  test('OnBoardingPageValidationSuccessCheckVisible', async () => {
    await initializeLandingPage();
    service.triggerHardwareVerificationStatusObserver(true, '', 0);
    await flushTasks();

    const busy = component.shadowRoot.querySelector('#busyIcon');
    const verification =
        component.shadowRoot.querySelector('#verificationIcon');
    const error = component.shadowRoot.querySelector('#errorMessage');
    assertFalse(isVisible(busy));
    assertTrue(isVisible(verification));
    assertEquals('shimless-icon:check', verification.icon);
    assertFalse(isVisible(error));
  });

  test('OnBoardingPageValidationFailedWarning', async () => {
    await initializeLandingPage();

    // Link should not be created before the failure occurs.
    assertFalse(
        !!component.shadowRoot.querySelector('#unqualifiedComponentsLink'));

    service.triggerHardwareVerificationStatusObserver(false, 'FAILURE', 0);
    await flushTasks();

    const busy = component.shadowRoot.querySelector('#busyIcon');
    const verification =
        component.shadowRoot.querySelector('#verificationIcon');
    const error = component.shadowRoot.querySelector('#errorMessage');
    assertTrue(busy.hidden);
    assertTrue(isVisible(verification));
    assertTrue(
        !!component.shadowRoot.querySelector('#unqualifiedComponentsLink'));
  });

  test('OnBoardingPageValidationFailedOpenDialog', async () => {
    await initializeLandingPage();

    const failedComponent = 'Keyboard';
    service.triggerHardwareVerificationStatusObserver(
        false, failedComponent, 0);
    await flushTasks();

    component.shadowRoot.querySelector('#unqualifiedComponentsLink').click();

    assertTrue(
        component.shadowRoot.querySelector('#unqualifiedComponentsDialog')
            .open);
    assertEquals(
        failedComponent,
        component.shadowRoot.querySelector('#dialogBody').textContent.trim());
  });

  test('OnBoardingPageExitButtonDispatchesExitEvent', async () => {
    await initializeLandingPage();

    let exitButtonEventFired = false;
    component.addEventListener('click-exit-button', (e) => {
      exitButtonEventFired = true;
    });

    await clickButton('#landingExit');
    await flushTasks();

    assertTrue(exitButtonEventFired);
  });

  test(
      'OnBoardingPageGetStartedButtonDispatchesTransitionStateEvent',
      async () => {
        await initializeLandingPage();

        let getStartedButtonEventFired = false;
        component.addEventListener('transition-state', (e) => {
          getStartedButtonEventFired = true;
        });

        const getStartedButton =
            component.shadowRoot.querySelector('#getStartedButton');
        getStartedButton.disabled = false;

        await clickButton('#getStartedButton');

        await flushTasks();

        assertTrue(getStartedButtonEventFired);
      });
});
