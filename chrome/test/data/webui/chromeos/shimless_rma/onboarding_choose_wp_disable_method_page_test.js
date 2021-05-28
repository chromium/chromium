// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseWpDisableMethodPageElement} from 'chrome://shimless-rma/onboarding_choose_wp_disable_method_page.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingChooseWpDisableMethodPageTest() {
  /** @type {?OnboardingChooseWpDisableMethodPageElement} */
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
   */
  function initializeChooseWpDisableMethodPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingChooseWpDisableMethodPageElement} */ (
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
}
