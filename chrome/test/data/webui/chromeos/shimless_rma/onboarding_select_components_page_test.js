// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeComponentsForRepairStateTest} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingSelectComponentsPageElement} from 'chrome://shimless-rma/onboarding_select_components_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {Component, ComponentRepairStatus} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function onboardingSelectComponentsPageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used by
   * the rework button.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?OnboardingSelectComponentsPageElement} */
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
   * @param {!Array<!Component>} deviceComponents
   * @return {!Promise}
   */
  function initializeComponentSelectPage(deviceComponents) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setGetComponentListResult(deviceComponents);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!OnboardingSelectComponentsPageElement} */ (
        document.createElement('onboarding-select-components-page'));
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
    assertTrue(!!cameraComponent);
    assertFalse(cameraComponent.disabled);
    cameraComponent.shadowRoot.querySelector('#componentButton').click();
    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickReworkButton() {
    const reworkFlowLink =
        component.shadowRoot.querySelector('#reworkFlowLink');
    assertTrue(!!reworkFlowLink);
    reworkFlowLink.click();
    return flushTasks();
  }

  /**
   * Get getComponentRepairStateList_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {!Array<!Component>}
   */
  function getComponentRepairStateList() {
    return component.getComponentRepairStateList_();
  }

  test('SelectComponentsPageInitializes', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    const reworkFlowLink =
        component.shadowRoot.querySelector('#reworkFlowLink');
    const cameraComponent =
        component.shadowRoot.querySelector('#componentCamera');
    const batteryComponent =
        component.shadowRoot.querySelector('#componentBattery');
    const touchpadComponent =
        component.shadowRoot.querySelector('#componentTouchpad');
    assertFalse(reworkFlowLink.hidden);
    assertEquals('Camera', cameraComponent.componentName);
    assertEquals('Camera_XYZ_1', cameraComponent.componentIdentifier);
    assertFalse(cameraComponent.disabled);
    assertFalse(cameraComponent.checked);
    assertEquals('Battery', batteryComponent.componentName);
    assertEquals('Battery_XYZ_Lithium', batteryComponent.componentIdentifier);
    assertTrue(batteryComponent.disabled);
    assertFalse(batteryComponent.checked);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertEquals('Touchpad_XYZ_2', touchpadComponent.componentIdentifier);
    assertFalse(touchpadComponent.disabled);
    assertTrue(touchpadComponent.checked);
  });

  test('SelectComponentsPageToggleComponent', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);
    await clickComponentCameraToggle();

    let components = getComponentRepairStateList();
    assertNotEquals(fakeComponentsForRepairStateTest, components);
    fakeComponentsForRepairStateTest[0].state = ComponentRepairStatus.kReplaced;
    assertDeepEquals(fakeComponentsForRepairStateTest, components);
  });

  test('SelectComponentsPageReworkCallsReworkMainboard', async () => {
    const resolver = new PromiseResolver();
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);
    let callCounter = 0;
    service.reworkMainboard = () => {
      callCounter++;
      return resolver.promise;
    };

    await clickReworkButton();

    assertEquals(1, callCounter);
  });

  test('SelectComponentsPageOnNextCallsSetComponentList', async () => {
    const resolver = new PromiseResolver();
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);
    let callCounter = 0;
    service.setComponentList = (components) => {
      assertDeepEquals(fakeComponentsForRepairStateTest, components);
      callCounter++;
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, callCounter);
    assertDeepEquals(expectedResult, savedResult);
  });

  test('SelectComponentsPageDisablesComponents', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    const cameraComponent =
        component.shadowRoot.querySelector('#componentCamera');
    const touchpadComponent =
        component.shadowRoot.querySelector('#componentTouchpad');
    assertFalse(cameraComponent.disabled);
    assertFalse(touchpadComponent.disabled);
    component.allButtonsDisabled = true;
    assertTrue(cameraComponent.disabled);
    assertTrue(touchpadComponent.disabled);
  });

  test('SelectComponentsPageReworkLinkDisabled', async () => {
    const resolver = new PromiseResolver();
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);
    let callCounter = 0;
    service.reworkMainboard = () => {
      callCounter++;
      return resolver.promise;
    };

    component.allButtonsDisabled = true;
    await clickReworkButton();

    assertEquals(0, callCounter);
  });
}
