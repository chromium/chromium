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
import {ReimagingCalibrationRunPage} from 'chrome://shimless-rma/reimaging_calibration_run_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationOverallStatus, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('reimagingCalibrationRunPageTest', function() {
  // ShimlessRma is needed to handle the 'transition-state' event used when
  // handling calibration overall progress signals.
  let shimlessRmaComponent: ShimlessRma|null = null;

  let component: ReimagingCalibrationRunPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
  });

  function initializeCalibrationRunPage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(ReimagingCalibrationRunPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify clicking the next button before a calibration completes is rejected
  // but succeeds after calibration.
  test('NextButtonBeforeAndAfterCalibration', async () => {
    await initializeCalibrationRunPage();

    assert(service);
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let calibrationCompleteCalls = 0;
    service.calibrationComplete = () => {
      ++calibrationCompleteCalls;
      return resolver.promise;
    };

    // Click the next button and expect a rejection.
    assert(component);
    let wasPromiseRejected = false;
    try {
      component.onNextButtonClick();
      assertNotReached('Do not proceed while calibration running.');
    } catch (error: unknown) {
      wasPromiseRejected = true;
    }
    assertTrue(wasPromiseRejected);
    let expectedCalls = 0;
    assertEquals(expectedCalls, calibrationCompleteCalls);

    // Trigger the calibration to complete.
    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, /* delayMs= */ 0);
    await flushTasks();

    // Click the next button again expecting it to succeed.
    component.onNextButtonClick();
    expectedCalls = 1;
    assertEquals(expectedCalls, calibrationCompleteCalls);
  });

  // Verify the calibration complete status updates the UI elements.
  test('CalibrationCompleteUpdatesUI', async () => {
    await initializeCalibrationRunPage();

    assert(component);
    const calibrationTitle =
        strictQuery('h1', component.shadowRoot, HTMLElement);
    const progressSpinner =
        strictQuery('paper-spinner-lite', component.shadowRoot, HTMLElement);
    const completeIllustration =
        strictQuery('img', component.shadowRoot, HTMLElement);

    assertEquals(
        loadTimeData.getString('runCalibrationTitleText'),
        calibrationTitle.textContent!.trim());
    assertFalse(progressSpinner.hidden);
    assertTrue(completeIllustration.hidden);

    // Trigger the calibration to complete.
    assert(service);
    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, /* delayMs= */ 0);
    await flushTasks();

    // The UI should update to be in calibration complete mode.
    assertEquals(
        loadTimeData.getString('runCalibrationCompleteTitleText'),
        calibrationTitle.textContent!.trim());
    assertTrue(progressSpinner.hidden);
    assertFalse(completeIllustration.hidden);
  });

  // Verify continue calibration is invoked for the correct calibration
  // statuses.
  test('CalibrationRoundCompleteContinueCalibration', async () => {
    await initializeCalibrationRunPage();

    assert(service);
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let continueCalibrationCalls = 0;
    service.continueCalibration = () => {
      ++continueCalibrationCalls;
      return resolver.promise;
    };

    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallCurrentRoundComplete,
        /* delayMs= */ 0);
    await flushTasks();
    let expectedCalls = 1;
    assertEquals(expectedCalls, continueCalibrationCalls);

    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallCurrentRoundFailed,
        /* delayMs= */ 0);
    await flushTasks();
    expectedCalls = 2;
    assertEquals(expectedCalls, continueCalibrationCalls);

    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallInitializationFailed,
        /* delayMs= */ 0);
    await flushTasks();
    expectedCalls = 3;
    assertEquals(expectedCalls, continueCalibrationCalls);

    // `kCalibrationOverallComplete` is not expected to continue
    // calibration.
    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, /* delayMs= */ 0);
    await flushTasks();
    assertEquals(expectedCalls, continueCalibrationCalls);
  });
});
