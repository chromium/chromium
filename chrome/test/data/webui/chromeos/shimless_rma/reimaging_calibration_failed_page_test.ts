// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CalibrationComponentChipElement} from 'chrome://shimless-rma/calibration_component_chip.js';
import {DISABLE_NEXT_BUTTON} from 'chrome://shimless-rma/events.js';
import {fakeCalibrationComponentsWithFails, fakeCalibrationComponentsWithoutFails} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationFailedPage} from 'chrome://shimless-rma/reimaging_calibration_failed_page.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// TODO(crbug.com/40214914): Add a non-flaky test for keyboard navigation.
suite('reimagingCalibrationFailedPageTest', function() {
  let component: ReimagingCalibrationFailedPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  const cameraSelector = '#componentCamera';
  const baseGyroscopeSelector = '#componentBaseGyroscope';
  const failedComponentsDialogSelector = '#failedComponentsDialog';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function initializeCalibrationPage(
      calibrationComponents: CalibrationComponentStatus[]): Promise<void> {
    // Initialize the fake data.
    assert(service);
    service.setGetCalibrationComponentListResult(calibrationComponents);

    assert(!component);
    component = document.createElement(ReimagingCalibrationFailedPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  function clickComponentCameraToggle(): Promise<void> {
    assert(component);
    strictQuery(
        cameraSelector, component.shadowRoot, CalibrationComponentChipElement)
        .click();
    return flushTasks();
  }

  function getComponentsList(): CalibrationComponentStatus[] {
    assert(component);
    return component.getComponentsListForTesting();
  }

  // Verify the page initializes with component chips in the expected state.
  test('Initializes', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    assert(component);
    const cameraComponent = strictQuery(
        cameraSelector, component.shadowRoot, CalibrationComponentChipElement);
    assertEquals('Camera', cameraComponent.componentName);
    assertFalse(cameraComponent.checked);
    assertTrue(cameraComponent.failed);
    assertFalse(cameraComponent.disabled);

    const batteryComponent = strictQuery(
        '#componentBattery', component.shadowRoot,
        CalibrationComponentChipElement);
    assertEquals('Battery', batteryComponent.componentName);
    assertFalse(batteryComponent.checked);
    assertFalse(batteryComponent.failed);
    assertTrue(batteryComponent.disabled);

    const baseAccelerometerComponent = strictQuery(
        '#componentBaseAccelerometer', component.shadowRoot,
        CalibrationComponentChipElement);
    assertEquals(
        'Base Accelerometer', baseAccelerometerComponent.componentName);
    assertFalse(baseAccelerometerComponent.checked);
    assertFalse(baseAccelerometerComponent.failed);
    assertTrue(baseAccelerometerComponent.disabled);

    const lidAccelerometerComponent = strictQuery(
        '#componentLidAccelerometer', component.shadowRoot,
        CalibrationComponentChipElement);
    assertEquals('Lid Accelerometer', lidAccelerometerComponent.componentName);
    assertFalse(lidAccelerometerComponent.checked);
    assertFalse(lidAccelerometerComponent.failed);
    assertTrue(lidAccelerometerComponent.disabled);

    const touchpadComponent = strictQuery(
        '#componentTouchpad', component.shadowRoot,
        CalibrationComponentChipElement);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertFalse(touchpadComponent.checked);
    assertFalse(touchpadComponent.failed);
    assertTrue(touchpadComponent.disabled);
  });

  // Verify clicking the Camera chip toggles it between states.
  test('ToggleComponent', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    // Click the camera button to check it.
    await clickComponentCameraToggle();
    let cameraComponent = getComponentsList().find(
        (calibrationComponent: CalibrationComponentStatus) =>
            calibrationComponent.component === ComponentType.kCamera);
    assert(cameraComponent);
    assertEquals(CalibrationStatus.kCalibrationWaiting, cameraComponent.status);

    // Click the camera button again to uncheck it.
    await clickComponentCameraToggle();
    cameraComponent = getComponentsList().find(
        (calibrationComponent: CalibrationComponentStatus) =>
            calibrationComponent.component === ComponentType.kCamera);
    assert(cameraComponent);
    assertEquals(CalibrationStatus.kCalibrationSkip, cameraComponent.status);
  });

  // Verify clicking the exit button triggers the calibration complete signal.
  test('ExitButtonTriggersCalibrationComplete', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithoutFails);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let startCalibrationCalls = 0;
    assert(service);
    service.startCalibration = (components: CalibrationComponentStatus[]) => {
      const expectedComponents = 5;
      assertEquals(expectedComponents, components.length);
      components.forEach(
          (component: CalibrationComponentStatus) => assertEquals(
              CalibrationStatus.kCalibrationComplete, component.status));
      ++startCalibrationCalls;
      return resolver.promise;
    };

    assert(component);
    component.onExitButtonClick();

    const expectedCalls = 1;
    assertEquals(expectedCalls, startCalibrationCalls);
  });

  // Verify clicking the next button triggers a new calibration.
  test('NextButtonTriggersCalibration', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    await clickComponentCameraToggle();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let startCalibrationCalls = 0;
    assert(service);
    service.startCalibration = (components: CalibrationComponentStatus[]) => {
      const expectedCompnents = 7;
      assertEquals(expectedCompnents, components.length);

      components.forEach((component: CalibrationComponentStatus) => {
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
      ++startCalibrationCalls;
      return resolver.promise;
    };

    assert(component);
    component.onNextButtonClick();

    const expectedCalls = 1;
    assertEquals(expectedCalls, startCalibrationCalls);
  });

  // Verify when `allButtonsDisabled` is set all component chips are disabled.
  test('ComponentChipAllButtonsDisabled', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    // Base Gyroscope is a failed component so it starts off not disabled.
    assert(component);
    const baseGyroscopeComponent = strictQuery(
        baseGyroscopeSelector, component.shadowRoot,
        CalibrationComponentChipElement);
    assertFalse(baseGyroscopeComponent.disabled);
    component.allButtonsDisabled = true;
    assertTrue(baseGyroscopeComponent.disabled);
  });

  // Verify attempting to skip a calibration with failed components is rejected
  // and opens a dialog.
  test('SkipCalibrationWithFailedComponents', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    // Click the skip/exit button and expect the request to be rejected and open
    // the confirmation dialog.
    assert(component);
    let wasPromiseRejected = false;
    try {
      await component.onExitButtonClick();
      assertNotReached('Do not proceed with failed components');
    } catch (error: unknown) {
      wasPromiseRejected = true;
    }
    assertTrue(wasPromiseRejected);
    const failedComponentsDialog = strictQuery(
        failedComponentsDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(failedComponentsDialog.open);

    // Click the skip button and expect the dialog to close.
    strictQuery('#dialogSkipButton', component.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();
    assertFalse(failedComponentsDialog.open);
  });

  // Verify clicking the dialog retry button restarts calibration.
  test('FailedComponentsDialogRetryButton', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let startCalibrationCalls = 0;
    assert(service);
    service.startCalibration = (/* components= */[]) => {
      ++startCalibrationCalls;
      return resolver.promise;
    };

    // Click the skip/exit button and expect the request to be rejected and open
    // the confirmation dialog.
    assert(component);
    try {
      await component.onExitButtonClick();
      assertNotReached('Do not proceed with failed components');
    } catch (error: unknown) {
    }

    // Click the retry button and expect the dialog to close without starting a
    // calibration.
    strictQuery('#dialogRetryButton', component.shadowRoot, CrButtonElement)
        .click();
    assertEquals(0, startCalibrationCalls);
    assertFalse(strictQuery(
                    failedComponentsDialogSelector, component.shadowRoot,
                    CrDialogElement)
                    .open);
  });

  // Verify that the next button is only enabled if at least one component is
  // selected.
  test('NextButtonOnlyEnabledIfComponentIsSelected', async () => {
    await initializeCalibrationPage(fakeCalibrationComponentsWithFails);

    assert(component);
    const disableNextButtonEvent =
        eventToPromise(DISABLE_NEXT_BUTTON, component);

    // Select a component and expect the next button to be enabled.
    assert(component);
    const componentBaseGyroscopeButton = strictQuery(
        baseGyroscopeSelector, component.shadowRoot,
        CalibrationComponentChipElement);
    componentBaseGyroscopeButton.click();
    const enableNextButtonResponse = await disableNextButtonEvent;
    assertFalse(enableNextButtonResponse.detail);

    // Select the component again to uncheck it so no components are selected
    // and expect the next button to be disabled.
    componentBaseGyroscopeButton.click();
    const disableNextButtonResponse = await disableNextButtonEvent;
    assertFalse(disableNextButtonResponse.detail);
  });
});
