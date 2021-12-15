// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeCalibrationComponents} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationFailedPage} from 'chrome://shimless-rma/reimaging_calibration_failed_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function reimagingCalibrationFailedPageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?ReimagingCalibrationFailedPage} */
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
    shimless_rma_component.remove();
    shimless_rma_component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   * @param {!Array<!CalibrationComponentStatus>} calibrationComponents
   */
  function initializeCalibrationPage(calibrationComponents) {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    // Initialize the fake data.
    service.setGetCalibrationComponentListResult(calibrationComponents);

    component = /** @type {!ReimagingCalibrationFailedPage} */ (
        document.createElement('reimaging-calibration-failed-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickComponentCameraToggle() {
    const cameraComponent =
        component.shadowRoot.querySelector('#componentCamera');
    cameraComponent.click();
    return flushTasks();
  }

  /** @return {!Promise} */
  function clickRetryCalibrationButton() {
    const retryButton =
        component.shadowRoot.querySelector('#retryCalibrationButton');
    assertFalse(retryButton.disabled);
    retryButton.click();
    return flushTasks();
  }

  /**
   * Get getComponentsList_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {!Array<!CalibrationComponentStatus>}
   */
  function getComponentsList() {
    return component.getComponentsList_();
  }


  test('Initializes', async () => {
    await initializeCalibrationPage(fakeCalibrationComponents);

    const cameraComponent =
        component.shadowRoot.querySelector('#componentCamera');
    const batteryComponent =
        component.shadowRoot.querySelector('#componentBattery');
    const baseAccelerometerComponent =
        component.shadowRoot.querySelector('#componentBaseAccelerometer');
    const lidAccelerometerComponent =
        component.shadowRoot.querySelector('#componentLidAccelerometer');
    const touchpadComponent =
        component.shadowRoot.querySelector('#componentTouchpad');
    assertEquals('Camera', cameraComponent.componentName);
    assertFalse(cameraComponent.checked);
    assertFalse(cameraComponent.failed);
    assertEquals('Battery', batteryComponent.componentName);
    assertFalse(batteryComponent.checked);
    assertFalse(batteryComponent.failed);
    assertEquals(
        'Base Accelerometer', baseAccelerometerComponent.componentName);
    assertFalse(baseAccelerometerComponent.checked);
    assertFalse(baseAccelerometerComponent.failed);
    assertEquals('Lid Accelerometer', lidAccelerometerComponent.componentName);
    assertFalse(lidAccelerometerComponent.checked);
    assertTrue(lidAccelerometerComponent.failed);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertFalse(touchpadComponent.checked);
    assertFalse(touchpadComponent.failed);
  });

  test('ToggleComponent', async () => {
    await initializeCalibrationPage(fakeCalibrationComponents);
    getComponentsList().forEach(
        component =>
            assertEquals(CalibrationStatus.kCalibrationSkip, component.status));

    // Click the camera button to check it.
    await clickComponentCameraToggle();
    // Camera should be the first entry in the list.
    assertEquals(
        CalibrationStatus.kCalibrationWaiting, getComponentsList()[0].status);

    // Click the camera button to check it.
    await clickComponentCameraToggle();
    // Camera should be the first entry in the list.
    assertEquals(
        CalibrationStatus.kCalibrationSkip, getComponentsList()[0].status);
  });

  test('NextButtonTriggersCalibrationComplete', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(fakeCalibrationComponents);
    let startCalibrationCalls = 0;
    service.startCalibration = (components) => {
      assertEquals(5, components.length);
      components.forEach(
          component => assertEquals(
              CalibrationStatus.kCalibrationSkip, component.status));
      startCalibrationCalls++;
      return resolver.promise;
    };
    await flushTasks();

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, startCalibrationCalls);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('RetryButtonTriggersCalibration', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(fakeCalibrationComponents);

    getComponentsList().forEach(
        component =>
            assertEquals(CalibrationStatus.kCalibrationSkip, component.status));
    await clickComponentCameraToggle();

    let startCalibrationCalls = 0;
    service.startCalibration = (components) => {
      assertEquals(5, components.length);
      components.forEach(
          component => assertEquals(
              component.component === ComponentType.kCamera ?
                  CalibrationStatus.kCalibrationWaiting :
                  CalibrationStatus.kCalibrationSkip,
              component.status));
      startCalibrationCalls++;
      return resolver.promise;
    };

    await clickRetryCalibrationButton();
    assertEquals(1, startCalibrationCalls);
  });
}
