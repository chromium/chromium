// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeComponentsForRepairStateTest} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingSelectComponentsPageElement} from 'chrome://shimless-rma/onboarding_select_components_page.js';
import {Component, ComponentRepairState} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingSelectComponentsPageTest() {
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

    component = /** @type {!OnboardingSelectComponentsPageElement} */ (
        document.createElement('onboarding-select-components-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickComponentKeyboardToggle() {
    const keyboardComponent =
        component.shadowRoot.querySelector('#componentKeyboard');
    assertFalse(keyboardComponent.disabled);
    keyboardComponent.click();
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

    const reworkFlowLink = component.shadowRoot.querySelector('#reworkFlow');
    const keyboardComponent =
        component.shadowRoot.querySelector('#componentKeyboard');
    const thumbReaderComponent =
        component.shadowRoot.querySelector('#componentThumbReader');
    const trackpadComponent =
        component.shadowRoot.querySelector('#componentTrackpad');
    assertFalse(reworkFlowLink.hidden);
    assertEquals(keyboardComponent.componentName, 'Keyboard');
    assertFalse(keyboardComponent.disabled);
    assertFalse(keyboardComponent.checked);
    assertEquals(thumbReaderComponent.componentName, 'Thumb Reader');
    assertTrue(thumbReaderComponent.disabled);
    assertFalse(thumbReaderComponent.checked);
    assertEquals(trackpadComponent.componentName, 'Trackpad');
    assertFalse(trackpadComponent.disabled);
    assertTrue(trackpadComponent.checked);
  });

  test('SelectComponentsPageToggleComponent', async () => {
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);
    await clickComponentKeyboardToggle();

    let components = getComponentRepairStateList();
    assertNotEquals(components, fakeComponentsForRepairStateTest);
    fakeComponentsForRepairStateTest[0].state = ComponentRepairState.kReplaced;
    assertDeepEquals(components, fakeComponentsForRepairStateTest);
  });

  // TODO(gavindodd): Add test of rework flow link when it does something.

  test('SelectComponentsPageOnNextCallsSetComponentList', async () => {
    const resolver = new PromiseResolver();
    await initializeComponentSelectPage(fakeComponentsForRepairStateTest);
    let callCounter = 0;
    service.setComponentList = (components) => {
      assertDeepEquals(components, fakeComponentsForRepairStateTest);
      callCounter++;
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertDeepEquals(savedResult, expectedResult);
  });
}
