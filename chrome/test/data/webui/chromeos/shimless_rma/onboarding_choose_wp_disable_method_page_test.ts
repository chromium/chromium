// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseWpDisableMethodPage} from 'chrome://shimless-rma/onboarding_choose_wp_disable_method_page.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingChooseWpDisableMethodPageTest', function() {
  let component: OnboardingChooseWpDisableMethodPage|null = null;

  let service: FakeShimlessRmaService|null = null;

  const manualSelector = '#hwwpDisableMethodManual';
  const rsuSelector = '#hwwpDisableMethodRsu';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    service = null;
  });

  function initializeChooseWpDisableMethodPage(): Promise<void> {
    assert(!component);
    component = document.createElement(OnboardingChooseWpDisableMethodPage.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  // Verify the page is initialized with both radio buttons unchecked.
  test('ChooseWpDisableMethodPageInitializes', async () => {
    await initializeChooseWpDisableMethodPage();
    assert(component);
    assertFalse(
        strictQuery(manualSelector, component.shadowRoot, CrRadioButtonElement)
            .checked);
    assertFalse(
        strictQuery(rsuSelector, component.shadowRoot, CrRadioButtonElement)
            .checked);
  });

  // Verify only one radio button can be checked at a time.
  test('ChooseWpDisableMethodPageOneChoiceOnly', async () => {
    await initializeChooseWpDisableMethodPage();
    assert(component);
    const manualDisableComponent =
        strictQuery(manualSelector, component.shadowRoot, CrRadioButtonElement);
    const rsuDisableComponent =
        strictQuery(rsuSelector, component.shadowRoot, CrRadioButtonElement);

    manualDisableComponent.click();
    assertTrue(manualDisableComponent.checked);
    assertFalse(rsuDisableComponent.checked);

    rsuDisableComponent.click();
    assertFalse(manualDisableComponent.checked);
    assertTrue(rsuDisableComponent.checked);
  });


  // Verify the correct function is called when Manual Disable is selected.
  test('SelectManuallyDisableWriteProtect', async () => {
    await initializeChooseWpDisableMethodPage();

    const expectedPromise = new PromiseResolver<{stateResult: StateResult}>();
    assert(service);
    service.setManuallyDisableWriteProtect = () => expectedPromise.promise;

    assert(component);
    const manualDisableComponent =
        strictQuery(manualSelector, component.shadowRoot, CrRadioButtonElement);
    manualDisableComponent.click();
    assertTrue(manualDisableComponent.checked);

    assertEquals(expectedPromise.promise, component.onNextButtonClick());
  });

  // Verify the correct function is called when RSU Disable is selected.
  test('SelectRsuDisableWriteProtect', async () => {
    await initializeChooseWpDisableMethodPage();

    const expectedPromise = new PromiseResolver<{stateResult: StateResult}>();
    assert(service);
    service.setRsuDisableWriteProtect = () => expectedPromise.promise;

    assert(component);
    const rsuDisableComponent =
        strictQuery(rsuSelector, component.shadowRoot, CrRadioButtonElement);
    rsuDisableComponent.click();
    assertTrue(rsuDisableComponent.checked);

    assertEquals(expectedPromise.promise, component.onNextButtonClick());
  });

  // Verify all the radio buttons are disabled when `allButtonsDisabled` is set.
  test('ChooseWpDisableMethodDisableRadioGroup', async () => {
    await initializeChooseWpDisableMethodPage();

    assert(component);
    const hwwpDisableMethodGroup = strictQuery(
        '#hwwpDisableMethod', component.shadowRoot, CrRadioGroupElement);
    assertFalse(hwwpDisableMethodGroup.disabled);
    component.allButtonsDisabled = true;
    assertTrue(hwwpDisableMethodGroup.disabled);
  });
});
