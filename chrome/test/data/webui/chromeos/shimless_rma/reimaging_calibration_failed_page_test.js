// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {fakeCalibrationComponentsWithFails, fakeCalibrationComponentsWithoutFails} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationFailedPage} from 'chrome://shimless-rma/reimaging_calibration_failed_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

// TODO(crbug/1296829): Add a non-flaky test for keyboard navigation.
suite('reimagingCalibrationFailedPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?ReimagingCalibrationFailedPage} */
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
   * @param {!Array<!CalibrationComponentStatus>} calibrationComponents
   */
  function initializeCalibrationPage(calibrationComponents) {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

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
    assertTrue(cameraComponent.failed);
    assertFalse(cameraComponent.disabled);
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
    assertFalse(lidAccelerometerComponent.failed);
    assertTrue(lidAccelerometerComponent.disabled);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertFalse(touchpadComponent.checked);
    assertFalse(touchpadComponent.failed);
    assertTrue(touchpadComponent.disabled);
  });

  test('ToggleComponent', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);
    const componentList = getComponentsList();
    assertEquals(
        3,
        componentList
            .filter(
                component =>
                    component.status === CalibrationStatus.kCalibrationSkip)
            .length);
    assertEquals(
        4,
        componentList
            .filter(
                component =>
                    component.status === CalibrationStatus.kCalibrationComplete)
            .length);

    // Click the camera button to check it.
    await clickComponentCameraToggle();
    // Camera should be the first entry in the list.
    assertEquals(
        CalibrationStatus.kCalibrationWaiting, getComponentsList()[0].status);

    // Click the camera button to uncheck it.
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
              CalibrationStatus.kCalibrationComplete, component.status));
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

    await clickComponentCameraToggle();

    let startCalibrationCalls = 0;
    service.startCalibration = (components) => {
      assertEquals(7, components.length);
      components.forEach(component => {
        let expectedStatus;
        if (component.component === ComponentType.kCamera) {
          expectedStatus = CalibrationStatus.kCalibrationWaiting;
        } else if (
            component.component === ComponentType.kScreen ||
            component.component === ComponentType.kBaseGyroscope) {
          expectedStatus = CalibrationStatus.kCalibrationSkip;
        } else {
          expectedStatus = CalibrationStatus.kCalibrationComplete;
        }
        assertEquals(expectedStatus, component.status);
      });
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

    // Base Gyroscope is a failed component so it starts off not disabled.
    const baseGyroscopeComponent =
        component.shadowRoot.querySelector('#componentBaseGyroscope');
    assertFalse(baseGyroscopeComponent.disabled);
    component.allButtonsDisabled = true;
    assertTrue(baseGyroscopeComponent.disabled);
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

  test('NextButtonIsOnlyEnabledIfAtLeastOneComponentIsSelected', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    let disableNextButtonEventFired = false;
    let disableNextButton = false;

    const componentBaseGyroscopeButton =
        component.shadowRoot.querySelector('#componentBaseGyroscope')
            .shadowRoot.querySelector('#componentButton');

    const disableHandler = (event) => {
      disableNextButtonEventFired = true;
      disableNextButton = event.detail;
    };

    component.addEventListener('disable-next-button', disableHandler);

    // If a component is selected, enable the next button.
    componentBaseGyroscopeButton.click();
    await flushTasks();
    assertTrue(disableNextButtonEventFired);
    assertFalse(disableNextButton);

    // If no components are selected, disable the next button.
    disableNextButtonEventFired = false;
    componentBaseGyroscopeButton.click();
    await flushTasks();
    assertTrue(disableNextButtonEventFired);
    assertTrue(disableNextButton);

    component.removeEventListener('disable-next-button', disableHandler);
  });
});
