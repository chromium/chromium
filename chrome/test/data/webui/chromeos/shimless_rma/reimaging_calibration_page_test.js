// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationPageElement} from 'chrome://shimless-rma/reimaging_calibration_page.js';
import {CalibrationComponent} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function reimagingCalibrationPageTest() {
  /** @type {?ReimagingCalibrationPageElement} */
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
   * @param {!CalibrationComponent} repairedComponent
   */
  function initializeCalibrationPage(repairedComponent) {
    assertFalse(!!component);

    component = /** @type {!ReimagingCalibrationPageElement} */ (
        document.createElement('reimaging-calibration-page'));
    assertTrue(!!component);
    component.repairedComponent = repairedComponent;
    document.body.appendChild(component);

    return flushTasks();
  }

  test('Initializes', async () => {
    await initializeCalibrationPage(CalibrationComponent.kAccelerometer);
    const preCalibration =
        component.shadowRoot.querySelector('#preCalibration');
    const Calibration = component.shadowRoot.querySelector('#calibration');
    assertTrue(Calibration.hidden);
    assertFalse(preCalibration.hidden);
  });

  test('NextButtonTriggersCalibration', async () => {
    await initializeCalibrationPage(CalibrationComponent.kAccelerometer);
    component.onNextButtonClick().catch((err) => void 0);

    const preCalibration =
        component.shadowRoot.querySelector('#preCalibration');
    const Calibration = component.shadowRoot.querySelector('#calibration');
    assertFalse(Calibration.hidden);
    assertTrue(preCalibration.hidden);
  });

  test('CalibrationComplete', async () => {
    await initializeCalibrationPage(CalibrationComponent.kAccelerometer);
    component.onNextButtonClick().catch((err) => void 0);
    await flushTasks();

    service.triggerCalibrationObserver(
        CalibrationComponent.kAccelerometer, 100, 0);
    await flushTasks();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(!!savedResult);
  });

  test('CalibrationInProgress', async () => {
    await initializeCalibrationPage(CalibrationComponent.kAccelerometer);
    component.onNextButtonClick().catch((err) => void 0);
    await flushTasks();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Calibration is not complete.');
    assertEquals(savedResult, undefined);
  });
}
