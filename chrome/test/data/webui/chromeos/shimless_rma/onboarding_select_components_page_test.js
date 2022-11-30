// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {fakeComponentsForRepairStateTest} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingSelectComponentsPageElement} from 'chrome://shimless-rma/onboarding_select_components_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {Component, ComponentRepairStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('onboardingSelectComponentsPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used by
   * the rework button.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?OnboardingSelectComponentsPageElement} */
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
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
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

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

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

    const components = getComponentRepairStateList();
    assertNotEquals(fakeComponentsForRepairStateTest, components);
    fakeComponentsForRepairStateTest[1].state = ComponentRepairStatus.kReplaced;
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

    const expectedResult = {foo: 'bar'};
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

  test('SelectComponentsPageKeyboardNavigationWorks', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    const componentCameraButton =
        component.shadowRoot.querySelector('#componentCamera')
            .shadowRoot.querySelector('#componentButton');
    const componentTouchpadButton =
        component.shadowRoot.querySelector('#componentTouchpad')
            .shadowRoot.querySelector('#componentButton');
    const componentNetworkButton =
        component.shadowRoot.querySelector('#componentNetwork')
            .shadowRoot.querySelector('#componentButton');
    // There are two cameras, so we can only get the first one by the id. We get
    // the second one by the unique id.
    const componentSecondCameraButton =
        component.shadowRoot.querySelector('[unique-id="7"]')
            .shadowRoot.querySelector('#componentButton');

    await flushTasks();

    componentCameraButton.click();
    assertDeepEquals(componentCameraButton, getDeepActiveElement());
    // We are at the beginning of the list, so left arrow should do nothing.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    await flushTasks();
    assertDeepEquals(componentCameraButton, getDeepActiveElement());

    // Skip the battery because it's missing.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    await flushTasks();
    assertDeepEquals(componentTouchpadButton, getDeepActiveElement());

    // Skip two components.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    await flushTasks();
    assertDeepEquals(componentNetworkButton, getDeepActiveElement());

    // If the next component is good, we don't skip it.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    await flushTasks();
    assertDeepEquals(componentSecondCameraButton, getDeepActiveElement());

    // We have reached the end of the list, so we can't go any further.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    await flushTasks();
    assertDeepEquals(componentSecondCameraButton, getDeepActiveElement());

    // Check that we can go backwards the same way.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    await flushTasks();
    assertDeepEquals(componentNetworkButton, getDeepActiveElement());
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    await flushTasks();
    assertDeepEquals(componentTouchpadButton, getDeepActiveElement());
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    await flushTasks();
    assertDeepEquals(componentCameraButton, getDeepActiveElement());

    // Check that the down button navigates down the column. It should skip the
    // network component, because it is in a different column.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await flushTasks();
    assertDeepEquals(componentTouchpadButton, getDeepActiveElement());
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await flushTasks();
    assertDeepEquals(componentSecondCameraButton, getDeepActiveElement());
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await flushTasks();
    assertDeepEquals(componentSecondCameraButton, getDeepActiveElement());

    // The up button should work in a similar way.
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    await flushTasks();
    assertDeepEquals(componentTouchpadButton, getDeepActiveElement());
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    await flushTasks();
    assertDeepEquals(componentCameraButton, getDeepActiveElement());
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    await flushTasks();
    assertDeepEquals(componentCameraButton, getDeepActiveElement());

    // Click on the touchpad button. It should come into focus.
    componentTouchpadButton.click();
    await flushTasks();
    assertDeepEquals(componentTouchpadButton, getDeepActiveElement());

    // Click on the battery button. It's disabled, so we shouldn't focus on it.
    const componentBatteryButton =
        component.shadowRoot.querySelector('#componentBattery')
            .shadowRoot.querySelector('#componentButton');
    componentBatteryButton.click();
    await flushTasks();
    assertDeepEquals(componentTouchpadButton, getDeepActiveElement());

    // Make sure we can bring both cameras into focus, even though they have the
    // same id.
    componentCameraButton.click();
    await flushTasks();
    assertDeepEquals(componentCameraButton, getDeepActiveElement());

    componentSecondCameraButton.click();
    await flushTasks();
    assertDeepEquals(componentSecondCameraButton, getDeepActiveElement());
  });
});
