// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {fakeComponentsForRepairStateTest} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingSelectComponentsPageElement} from 'chrome://shimless-rma/onboarding_select_components_page.js';
import {RepairComponentChip} from 'chrome://shimless-rma/repair_component_chip.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {Component, ComponentRepairStatus, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingSelectComponentsPageTest', function() {
  let component: OnboardingSelectComponentsPageElement|null = null;

  let service: FakeShimlessRmaService|null = null;

  // ShimlessRma is needed to handle the 'transition-state' event used by the
  // rework button.
  let shimlessRmaComponent: ShimlessRma|null = null;

  const componentButtonSelector = '#componentButton';
  const reworkFlowLinkSelector = '#reworkFlowLink';
  const cameraSelector = '#componentCamera';
  const batterySelector = '#componentBattery';
  const touchpadSelector = '#componentTouchpad';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
    service = null;
  });

  function initializeComponentSelectPage(deviceComponents: Component[]):
      Promise<void> {
    // Initialize the fake data.
    assert(service);
    service.setGetComponentListResult(deviceComponents);

    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component =
        document.createElement(OnboardingSelectComponentsPageElement.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  function clickComponentCameraToggle(): Promise<void> {
    assert(component);
    const cameraComponent =
        strictQuery(cameraSelector, component.shadowRoot, RepairComponentChip);
    assertFalse(cameraComponent.disabled);
    strictQuery(
        componentButtonSelector, cameraComponent.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  function clickReworkButton(): Promise<void> {
    assert(component);
    strictQuery(reworkFlowLinkSelector, component.shadowRoot, HTMLElement)
        .click();
    return flushTasks();
  }

  // Verify the page is initialized with the expected components.
  test('PageInitializes', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    assert(component);
    const reworkFlowLink =
        strictQuery(reworkFlowLinkSelector, component.shadowRoot, HTMLElement);
    assertFalse(reworkFlowLink.hidden);

    const cameraComponent =
        strictQuery(cameraSelector, component.shadowRoot, RepairComponentChip);
    assertEquals('Camera', cameraComponent.componentName);
    assertEquals('Camera_XYZ_1', cameraComponent.componentIdentifier);
    assertFalse(cameraComponent.disabled);
    assertFalse(cameraComponent.checked);

    const batteryComponent =
        strictQuery(batterySelector, component.shadowRoot, RepairComponentChip);
    assertEquals('Battery', batteryComponent.componentName);
    assertEquals('Battery_XYZ_Lithium', batteryComponent.componentIdentifier);
    assertTrue(batteryComponent.disabled);
    assertFalse(batteryComponent.checked);

    const touchpadComponent = strictQuery(
        touchpadSelector, component.shadowRoot, RepairComponentChip);
    assertEquals('Touchpad', touchpadComponent.componentName);
    assertEquals('Touchpad_XYZ_2', touchpadComponent.componentIdentifier);
    assertFalse(touchpadComponent.disabled);
    assertTrue(touchpadComponent.checked);
  });

  // Verify toggling a component updates its state.
  test('ToggleComponent', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    assert(component);
    let cameraComponent =
        component.getComponentRepairStateListForTesting().find(
            (repairComponent: Component) =>
                repairComponent.identifier === 'Camera_XYZ_1');
    assert(cameraComponent);
    assertEquals(ComponentRepairStatus.kOriginal, cameraComponent.state);

    // Click the camera component button and expect the state to change.
    await clickComponentCameraToggle();
    cameraComponent = component.getComponentRepairStateListForTesting().find(
        (repairComponent: Component) =>
            repairComponent.identifier === 'Camera_XYZ_1');
    assert(cameraComponent);
    assertEquals(ComponentRepairStatus.kReplaced, cameraComponent.state);
  });

  // Verify clicking the rework link makes the correct call.
  test('ReworkCallsReworkMainboard', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCounter = 0;
    assert(service);
    service.reworkMainboard = () => {
      ++callCounter;
      return resolver.promise;
    };

    await clickReworkButton();

    const expectedCallCount = 1;
    assertEquals(expectedCallCount, callCounter);
  });

  // Verify clicking the next button sets the component list.
  test('OnNextCallsSetComponentList', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCounter = 0;
    assert(service);
    service.setComponentList = (components: Component[]) => {
      assertDeepEquals(fakeComponentsForRepairStateTest, components);
      ++callCounter;
      return resolver.promise;
    };

    assert(component);
    component.onNextButtonClick();
    await resolver;

    const expectedCallCount = 1;
    assertEquals(expectedCallCount, callCounter);
  });

  // Verify all components are disabled when `allButtonsDisabled` is set.
  test('DisablesComponents', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    assert(component);
    // Select the two known enabled components and verify they become disabled.
    const cameraComponent =
        strictQuery(cameraSelector, component.shadowRoot, RepairComponentChip);
    const touchpadComponent = strictQuery(
        touchpadSelector, component.shadowRoot, RepairComponentChip);
    assertFalse(cameraComponent.disabled);
    assertFalse(touchpadComponent.disabled);

    component.allButtonsDisabled = true;
    assertTrue(cameraComponent.disabled);
    assertTrue(touchpadComponent.disabled);
  });

  // Verify clicking rework link has no response when `allButtonsDisabled` is
  // set.
  test('ReworkLinkDisabled', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCounter = 0;
    assert(service);
    service.reworkMainboard = () => {
      ++callCounter;
      return resolver.promise;
    };

    assert(component);
    component.allButtonsDisabled = true;
    await clickReworkButton();

    const expectedCallCount = 0;
    assertEquals(expectedCallCount, callCounter);
  });
});
