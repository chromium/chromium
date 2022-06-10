// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeCalibrationComponentsWithFails, fakeCalibrationComponentsWithoutFails} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationFailedPage} from 'chrome://shimless-rma/reimaging_calibration_failed_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNotReached, assertTrue} from '../../chai_assert.js';
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

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
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

  /**
   * Get getComponentsList_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {!Array<!CalibrationComponentStatus>}
   */
  function getComponentsList() {
    return component.getComponentsList_();
  }


  test('Initializes', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

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
    assertTrue(cameraComponent.disabled);
    assertEquals('Battery', batteryComponent.componentName);
    assertFalse(batteryComponent.checked);
    assertFalse(batteryComponent.failed);
    assertTrue(batteryComponent.disabled);
    assertEquals(
        'Base Accelerometer', baseAccelerometerComponent.componentName);
    assertFalse(baseAccelerometerComponent.checked);
    assertFalse(baseAccelerometerComponent.failed);
    assertTrue(baseAccelerometerComponent.disabled);
    assertEquals('Lid Accelerometer', lidAccelerometerComponent.componentName);
    assertFalse(lidAccelerometerComponent.checked);
    assertTrue(lidAccelerometerComponent.failed);
    assertFalse(lidAccelerometerComponent.disabled);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertFalse(touchpadComponent.checked);
    assertFalse(touchpadComponent.failed);
    assertTrue(touchpadComponent.disabled);
  });

  test('ToggleComponent', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);
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

  test('ExitButtonTriggersCalibrationComplete', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(fakeCalibrationComponentsWithoutFails);
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

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onExitButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, startCalibrationCalls);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('NextButtonTriggersCalibration', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

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

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, startCalibrationCalls);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('ComponentChipAllButtonsDisabled', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    // Lid Accelerometer is a failed component so it starts off not disabled.
    const lidAccelerometerComponent =
        component.shadowRoot.querySelector('#componentLidAccelerometer');
    assertFalse(lidAccelerometerComponent.disabled);
    component.allButtonsDisabled = true;
    assertTrue(lidAccelerometerComponent.disabled);
  });

  test('SkipCalibrationWithFailedComponents', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    let wasPromiseRejected = false;
    component.onExitButtonClick()
        .then(() => assertNotReached('Do not proceed with failed components'))
        .catch(() => {
          wasPromiseRejected = true;
        });

    await flushTasks();
    assertTrue(wasPromiseRejected);
  });

  test('FailedComponentsDialogSkipButton', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    const resolver = new PromiseResolver();
    let startCalibrationCalls = 0;
    service.startCalibration = (components) => {
      startCalibrationCalls++;
      return resolver.promise;
    };

    component.onExitButtonClick().catch(() => {});

    await flushTasks();
    assertEquals(0, startCalibrationCalls);
    assertTrue(
        component.shadowRoot.querySelector('#failedComponentsDialog').open);
    component.shadowRoot.querySelector('#dialogSkipButton').click();

    await flushTasks();
    assertEquals(1, startCalibrationCalls);
    assertFalse(
        component.shadowRoot.querySelector('#failedComponentsDialog').open);
  });

  test('FailedComponentsDialogRetryButton', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    const resolver = new PromiseResolver();
    let startCalibrationCalls = 0;
    service.startCalibration = (components) => {
      startCalibrationCalls++;
      return resolver.promise;
    };

    component.onExitButtonClick().catch(() => {});

    await flushTasks();
    assertEquals(0, startCalibrationCalls);
    assertTrue(
        component.shadowRoot.querySelector('#failedComponentsDialog').open);
    component.shadowRoot.querySelector('#dialogRetryButton').click();

    await flushTasks();
    assertEquals(0, startCalibrationCalls);
    assertFalse(
        component.shadowRoot.querySelector('#failedComponentsDialog').open);
  });
}
