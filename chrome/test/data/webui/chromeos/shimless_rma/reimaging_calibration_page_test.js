// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeCalibrationComponents} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationPageElement} from 'chrome://shimless-rma/reimaging_calibration_page.js';
import {CalibrationComponentStatus, CalibrationStatus} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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
   * @param {!Array<!CalibrationComponentStatus>} calibrationComponents
   */
  function initializeCalibrationPage(calibrationComponents) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setGetCalibrationComponentListResult(calibrationComponents);

    component = /** @type {!ReimagingCalibrationPageElement} */ (
        document.createElement('reimaging-calibration-page'));
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
    assertFalse(cameraComponent.disabled);
    cameraComponent.click();
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

  /**
   * @param {!Array<!CalibrationComponentStatus>} components
   * @return {!Array<!CalibrationComponentStatus>}
   */
  function getExpectedComponentsList(components) {
    /** @type {!Array<!CalibrationComponentStatus>} */
    let expectedComponents = [];
    components.forEach(componentStatus => {
      let status = componentStatus.status;
      if (status === CalibrationStatus.kCalibrationFailed) {
        status = CalibrationStatus.kCalibrationWaiting;
      }
      expectedComponents.push({
        component: componentStatus.component,
        status: status,
        progress: 0.0
      });
    });
    return expectedComponents;
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
    assertFalse(cameraComponent.disabled);
    assertFalse(cameraComponent.skip);
    assertFalse(cameraComponent.completed);
    assertEquals('Battery', batteryComponent.componentName);
    assertTrue(batteryComponent.disabled);
    assertFalse(batteryComponent.skip);
    assertTrue(batteryComponent.completed);
    assertEquals(
        'Base Accelerometer', baseAccelerometerComponent.componentName);
    assertTrue(baseAccelerometerComponent.disabled);
    assertFalse(baseAccelerometerComponent.skip);
    assertFalse(baseAccelerometerComponent.completed);
    assertEquals('Lid Accelerometer', lidAccelerometerComponent.componentName);
    assertFalse(lidAccelerometerComponent.disabled);
    assertFalse(lidAccelerometerComponent.skip);
    assertFalse(lidAccelerometerComponent.completed);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertFalse(touchpadComponent.disabled);
    assertTrue(touchpadComponent.skip);
    assertFalse(touchpadComponent.completed);
  });

  test('ToggleComponent', async () => {
    await initializeCalibrationPage(fakeCalibrationComponents);
    await clickComponentCameraToggle();
    let expectedComponents =
        getExpectedComponentsList(fakeCalibrationComponents);
    let components = getComponentsList();
    assertNotEquals(expectedComponents, components);
    // Camera should be the first entry in the list.
    expectedComponents[0].status = CalibrationStatus.kCalibrationSkip;
    assertDeepEquals(expectedComponents, components);
  });

  test('NextButtonTriggersCalibration', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(fakeCalibrationComponents);
    let expectedComponents =
        getExpectedComponentsList(fakeCalibrationComponents);
    let startCalibrationCalls = 0;
    service.startCalibration = (components) => {
      assertDeepEquals(expectedComponents, components);
      startCalibrationCalls++;
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, startCalibrationCalls);
    assertDeepEquals(expectedResult, savedResult);
  });
}
