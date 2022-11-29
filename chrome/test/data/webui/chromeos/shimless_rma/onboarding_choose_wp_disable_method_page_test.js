// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseWpDisableMethodPage} from 'chrome://shimless-rma/onboarding_choose_wp_disable_method_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingChooseWpDisableMethodPageTest', function() {
  /** @type {?OnboardingChooseWpDisableMethodPage} */
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
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeChooseWpDisableMethodPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingChooseWpDisableMethodPage} */ (
        document.createElement('onboarding-choose-wp-disable-method-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ChooseWpDisableMethodPageInitializes', async () => {
    await initializeChooseWpDisableMethodPage();
    const manualDisableComponent =
        component.shadowRoot.querySelector('#hwwpDisableMethodManual');
    const rsuDisableComponent =
        component.shadowRoot.querySelector('#hwwpDisableMethodRsu');

    assertFalse(manualDisableComponent.checked);
    assertFalse(rsuDisableComponent.checked);
  });

  test('ChooseWpDisableMethodPageOneChoiceOnly', async () => {
    await initializeChooseWpDisableMethodPage();
    const manualDisableComponent =
        component.shadowRoot.querySelector('#hwwpDisableMethodManual');
    const rsuDisableComponent =
        component.shadowRoot.querySelector('#hwwpDisableMethodRsu');

    manualDisableComponent.click();
    await flushTasks;

    assertTrue(manualDisableComponent.checked);
    assertFalse(rsuDisableComponent.checked);

    rsuDisableComponent.click();
    await flushTasks;

    assertFalse(manualDisableComponent.checked);
    assertTrue(rsuDisableComponent.checked);
  });


  test('SelectManuallyDisableWriteProtect', async () => {
    const resolver = new PromiseResolver();
    await initializeChooseWpDisableMethodPage();
    let callCounter = 0;
    service.chooseManuallyDisableWriteProtect = () => {
      callCounter++;
      return resolver.promise;
    };
    const manualDisableComponent =
        component.shadowRoot.querySelector('#hwwpDisableMethodManual');
    manualDisableComponent.click();
    await flushTasks;

    assertTrue(manualDisableComponent.checked);

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('SelectRsuDisableWriteProtect', async () => {
    const resolver = new PromiseResolver();
    await initializeChooseWpDisableMethodPage();
    let callCounter = 0;
    service.chooseRsuDisableWriteProtect = () => {
      callCounter++;
      return resolver.promise;
    };
    const rsuDisableComponent =
        component.shadowRoot.querySelector('#hwwpDisableMethodRsu');

    rsuDisableComponent.click();
    await flushTasks;
    assertTrue(rsuDisableComponent.checked);

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('ChooseWpDisableMethodDisableRadioGroup', async () => {
    await initializeChooseWpDisableMethodPage();

    const hwwpDisableMethodGroup =
        component.shadowRoot.querySelector('#hwwpDisableMethod');
    assertFalse(hwwpDisableMethodGroup.disabled);
    component.allButtonsDisabled = true;
    assertTrue(hwwpDisableMethodGroup.disabled);
  });
});
