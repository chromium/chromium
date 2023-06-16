// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationRunPage} from 'chrome://shimless-rma/reimaging_calibration_run_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationOverallStatus, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('reimagingCalibrationRunPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?ReimagingCalibrationRunPage} */
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
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeCalibrationRunPage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!ReimagingCalibrationRunPage} */ (
        document.createElement('reimaging-calibration-run-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('NextButtonBeforeCalibrationCompleteFails', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationRunPage();
    let calibrationCompleteCalls = 0;
    service.calibrationComplete = () => {
      calibrationCompleteCalls++;
      return resolver.promise;
    };

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertEquals(0, calibrationCompleteCalls);
    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Calibration is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('NextButtonAfterCalibrationCompleteTriggersContinue', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationRunPage();

    const calibrationTitle = component.shadowRoot.querySelector('h1');
    const progressSpinner =
        component.shadowRoot.querySelector('paper-spinner-lite');
    const completeIllustration = component.shadowRoot.querySelector('img');

    assertEquals(
        loadTimeData.getString('runCalibrationTitleText'),
        calibrationTitle.textContent.trim());
    assertFalse(progressSpinner.hidden);
    assertTrue(completeIllustration.hidden);

    let calibrationCompleteCalls = 0;
    service.calibrationComplete = () => {
      calibrationCompleteCalls++;
      return resolver.promise;
    };
    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, 0);
    await flushTasks();

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, calibrationCompleteCalls);
    assertDeepEquals(savedResult, expectedResult);
    assertEquals(
        loadTimeData.getString('runCalibrationCompleteTitleText'),
        calibrationTitle.textContent.trim());
    assertTrue(progressSpinner.hidden);
    assertFalse(completeIllustration.hidden);
  });

  test(
      'CalibrationOverallProgressRoundCompleteCallsContinueCalibration',
      async () => {
        const resolver = new PromiseResolver();
        await initializeCalibrationRunPage();
        let continueCalibrationCalls = 0;
        service.continueCalibration = () => {
          continueCalibrationCalls++;
          return resolver.promise;
        };
        service.triggerCalibrationOverallObserver(
            CalibrationOverallStatus.kCalibrationOverallCurrentRoundComplete,
            0);
        await flushTasks();

        assertEquals(1, continueCalibrationCalls);
      });

  test(
      'CalibrationOverallProgressRoundFailedCallsContinueCalibration',
      async () => {
        const resolver = new PromiseResolver();
        await initializeCalibrationRunPage();
        let continueCalibrationCalls = 0;
        service.continueCalibration = () => {
          continueCalibrationCalls++;
          return resolver.promise;
        };
        service.triggerCalibrationOverallObserver(
            CalibrationOverallStatus.kCalibrationOverallCurrentRoundFailed, 0);
        await flushTasks();

        assertEquals(1, continueCalibrationCalls);
      });

  test(
      'CalibrationOverallProgressIniitalizationFailedCallsContinueCalibration',
      async () => {
        const resolver = new PromiseResolver();
        await initializeCalibrationRunPage();
        let continueCalibrationCalls = 0;
        service.continueCalibration = () => {
          continueCalibrationCalls++;
          return resolver.promise;
        };
        service.triggerCalibrationOverallObserver(
            CalibrationOverallStatus.kCalibrationOverallInitializationFailed,
            0);
        await flushTasks();

        assertEquals(1, continueCalibrationCalls);
      });
});
