// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeChromeVersion, fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ButtonState, ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {RmadErrorCode, State, StateResult} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

/**
 * onSelected*Change is not triggered automatically and the functions are
 * protected. It is not possible to suppress visibility inline so this helper
 * function wraps them.
 * @suppress {visibility}
 * @return {!RmadErrorCode}
 */
function suppressedErrorMessage(component) {
  return component.errorMessage_;
}

export function shimlessRMAAppTest() {
  /** @type {?ShimlessRma} */
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
    document.body.innerHTML = '';
    service.reset();
  });

  /**
   * @param {!Array<!StateResult>} states
   * @param {string} chromeVersion
   */
  function initializeShimlessRMAApp(states, chromeVersion) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setStates(states);
    service.setGetCurrentOsVersionResult(chromeVersion);

    component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * Utility function to assert navigation buttons
   * TODO(joonbug): expand to cover assertion of ButtonState
   */
  function assertNavButtons() {
    const nextButton = component.shadowRoot.querySelector('#next');
    const prevButton = component.shadowRoot.querySelector('#cancel');
    const backButton = component.shadowRoot.querySelector('#back');
    assertTrue(!!nextButton);
    assertTrue(!!prevButton);
    assertTrue(!!backButton);
  }

  /**
   * Utility function to click next button
   * @return {Promise}
   */
  function clickNext() {
    const nextButton = component.shadowRoot.querySelector('#next');
    nextButton.click();
    return flushTasks();
  }

  /**
   * Utility function to click back button
   * @return {Promise}
   */
  function clickBack() {
    const backButton = component.shadowRoot.querySelector('#back');
    backButton.click();
    return flushTasks();
  }

  /**
   * Utility function to click cancel button
   * @return {Promise}
   */
  function clickCancel() {
    const cancelButton = component.shadowRoot.querySelector('#cancel');
    cancelButton.click();
    return flushTasks();
  }

  test('ShimlessRMALoaded', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    assertNavButtons();
  });

  test('ShimlessRMABasicNavigation', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const prevButton = component.shadowRoot.querySelector('#back');
    const cancelButton = component.shadowRoot.querySelector('#cancel');
    assertTrue(!!prevButton);
    assertTrue(!!cancelButton);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);
    assertFalse(initialPage.hidden);
    assertTrue(prevButton.hidden);
    assertFalse(cancelButton.hidden);

    // This enables the next button on the landing page.
    service.triggerHardwareVerificationStatusObserver(true, '', 0);
    await flushTasks();
    await clickNext();

    const selectNetworkPage =
        component.shadowRoot.querySelector('onboarding-network-page');
    assertTrue(!!selectNetworkPage);
    assertFalse(selectNetworkPage.hidden);
    assertTrue(!!initialPage);
    assertTrue(initialPage.hidden);
    assertFalse(prevButton.hidden);
    assertFalse(cancelButton.hidden);

    prevButton.click();
    await flushTasks();

    // components page should not be destroyed.
    assertTrue(!!selectNetworkPage);
    assertTrue(selectNetworkPage.hidden);
    assertFalse(initialPage.hidden);
    assertTrue(prevButton.hidden);
    assertFalse(cancelButton.hidden);
  });

  test('ShimlessRMACancellation', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    let abortRmaCount = 0;
    service.abortRma = () => {
      abortRmaCount++;
      return Promise.resolve(RmadErrorCode.kOk);
    };
    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    const cancelButton = component.shadowRoot.querySelector('#cancel');

    cancelButton.click();
    await flushTasks();

    assertEquals(1, abortRmaCount);
  });

  test('NextButtonClickedOnReady', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);

    const resolver = new PromiseResolver();
    initialPage.onNextButtonClick = () => resolver.promise;

    await clickNext();
    assertFalse(initialPage.hidden);

    resolver.resolve({state: State.kUpdateOs, error: RmadErrorCode.kOk});
    await flushTasks();

    const updatePage =
        component.shadowRoot.querySelector('onboarding-update-page');
    assertTrue(!!updatePage);
    assertFalse(updatePage.hidden);
    assertTrue(initialPage.hidden);
  });

  test('NextButtonClickedOnNotReady', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);

    const resolver = new PromiseResolver();
    initialPage.onNextButtonClick = () => resolver.promise;

    await clickNext();
    assertFalse(initialPage.hidden);

    resolver.reject();
    await flushTasks();

    assertFalse(initialPage.hidden);
  });

  test('UpdateButtonState', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const backButton = component.shadowRoot.querySelector('#back');
    assertTrue(!!backButton);
    assertTrue(backButton.hidden);

    component.updateButtonState('buttonBack', ButtonState.VISIBLE);
    await flushTasks();

    assertFalse(backButton.hidden);
  });

  test('UpdateNextButtonLabel', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canCancel: true,
          canGoBack: true,
          error: RmadErrorCode.kOk
        }],
        fakeChromeVersion[0]);

    const nextButton = component.shadowRoot.querySelector('#nextButtonLabel');
    assertEquals(
        loadTimeData.getString('nextButtonLabel'),
        nextButton.textContent.trim());

    component.dispatchEvent(new CustomEvent(
        'set-next-button-label',
        {bubbles: true, composed: true, detail: 'skipButtonLabel'},
        ));
    assertEquals(
        loadTimeData.getString('skipButtonLabel'),
        nextButton.textContent.trim());
  });

  test('ErrorSignalShowsErrorCode', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canCancel: true,
          canGoBack: true,
          error: RmadErrorCode.kOk
        }],
        fakeChromeVersion[0]);

    service.triggerErrorObserver(RmadErrorCode.kReimagingUsbInvalidImage, 0);

    await flushTasks();

    assertEquals(
        'Error: kReimagingUsbInvalidImage(23)',
        suppressedErrorMessage(component));
  });

  test('NextButtonSpinner', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canCancel: true,
          canGoBack: true,
          error: RmadErrorCode.kOk
        }],
        fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-select-components-page');
    assertTrue(!!initialPage);

    const nextButtonSpinner =
        component.shadowRoot.querySelector('#nextButtonSpinner');
    const backButtonSpinner =
        component.shadowRoot.querySelector('#backButtonSpinner');
    const cancelButtonSpinner =
        component.shadowRoot.querySelector('#cancelButtonSpinner');

    // Next spinner
    const nextResolver = new PromiseResolver();
    initialPage.onNextButtonClick = () => nextResolver.promise;
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(cancelButtonSpinner.hidden);

    await clickNext();
    assertFalse(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(cancelButtonSpinner.hidden);

    nextResolver.resolve({state: State.kUpdateOs, error: RmadErrorCode.kOk});
    await flushTasks();

    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(cancelButtonSpinner.hidden);
  });

  test('BackButtonSpinner', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canCancel: true,
          canGoBack: true,
          error: RmadErrorCode.kOk
        }],
        fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-select-components-page');
    assertTrue(!!initialPage);

    const nextButtonSpinner =
        component.shadowRoot.querySelector('#nextButtonSpinner');
    const backButtonSpinner =
        component.shadowRoot.querySelector('#backButtonSpinner');
    const cancelButtonSpinner =
        component.shadowRoot.querySelector('#cancelButtonSpinner');

    // Back spinner
    const backResolver = new PromiseResolver();
    service.transitionPreviousState = () => {
      return backResolver.promise;
    };
    await clickBack();
    assertTrue(nextButtonSpinner.hidden);
    assertFalse(backButtonSpinner.hidden);
    assertTrue(cancelButtonSpinner.hidden);

    backResolver.resolve({state: State.kUpdateOs, error: RmadErrorCode.kOk});
    await flushTasks();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(cancelButtonSpinner.hidden);
  });

  test('CancelButtonSpinner', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canCancel: true,
          canGoBack: true,
          error: RmadErrorCode.kOk
        }],
        fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-select-components-page');
    assertTrue(!!initialPage);

    const nextButtonSpinner =
        component.shadowRoot.querySelector('#nextButtonSpinner');
    const backButtonSpinner =
        component.shadowRoot.querySelector('#backButtonSpinner');
    const cancelButtonSpinner =
        component.shadowRoot.querySelector('#cancelButtonSpinner');

    // Cancel spinner
    const cancelResolver = new PromiseResolver();
    service.abortRma = () => {
      return cancelResolver.promise;
    };
    await clickCancel();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertFalse(cancelButtonSpinner.hidden);

    cancelResolver.resolve({state: State.kUpdateOs, error: RmadErrorCode.kOk});
    await flushTasks();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(cancelButtonSpinner.hidden);
  });
}
