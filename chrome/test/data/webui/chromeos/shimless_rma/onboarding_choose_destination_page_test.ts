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
import {OnboardingChooseDestinationPageElement} from 'chrome://shimless-rma/onboarding_choose_destination_page.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';


suite('onboardingChooseDestinationPageTest', function() {
  let component: OnboardingChooseDestinationPageElement|null = null;

  let service: FakeShimlessRmaService|null = null;

  const destinationOriginalOwnerSelector = '#destinationOriginalOwner';
  const destinationNewOwnerSelector = '#destinationNewOwner';
  const destinationNotSureOwnerSelector = '#destinationNotSureOwner';

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

  function initializeChooseDestinationPage(): Promise<void> {
    assert(!component);
    component =
        document.createElement(OnboardingChooseDestinationPageElement.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  // Verify the page initializes without any radio buttons checked.
  test('ChooseDestinationPageInitializes', async () => {
    await initializeChooseDestinationPage();
    assert(component);
    assertFalse(strictQuery(
                    destinationOriginalOwnerSelector, component.shadowRoot,
                    CrRadioButtonElement)
                    .checked);
    assertFalse(strictQuery(
                    destinationNewOwnerSelector, component.shadowRoot,
                    CrRadioButtonElement)
                    .checked);
    assertFalse(strictQuery(
                    destinationNotSureOwnerSelector, component.shadowRoot,
                    CrRadioButtonElement)
                    .checked);
  });

  // Verify only one radio button can be selected at once.
  test('ChooseDestinationPageOneChoiceOnly', async () => {
    await initializeChooseDestinationPage();
    assert(component);
    const originalOwnerComponent = strictQuery(
        destinationOriginalOwnerSelector, component.shadowRoot,
        CrRadioButtonElement);
    const newOwnerComponent = strictQuery(
        destinationNewOwnerSelector, component.shadowRoot,
        CrRadioButtonElement);
    const notSureOwnerComponent = strictQuery(
        destinationNotSureOwnerSelector, component.shadowRoot,
        CrRadioButtonElement);

    originalOwnerComponent.click();
    assertTrue(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);
    assertFalse(notSureOwnerComponent.checked);

    newOwnerComponent.click();
    assertFalse(originalOwnerComponent.checked);
    assertTrue(newOwnerComponent.checked);
    assertFalse(notSureOwnerComponent.checked);

    notSureOwnerComponent.click();
    assertFalse(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);
    assertTrue(notSureOwnerComponent.checked);
  });

  // Verify the correct function is called when original owner is selected.
  test('ChooseDestinationPageSameOwnerOnNextCallsSetSameOwner', async () => {
    await initializeChooseDestinationPage();
    const expectedPromise = new PromiseResolver<{stateResult: StateResult}>();
    assert(service);
    service.setSameOwner = () => expectedPromise.promise;
    assert(component);
    const originalOwnerComponent = strictQuery(
        destinationOriginalOwnerSelector, component.shadowRoot,
        CrRadioButtonElement);

    originalOwnerComponent.click();
    assertTrue(originalOwnerComponent.checked);
    assertEquals(expectedPromise.promise, component.onNextButtonClick());
  });

  // Verify the correct function is called when new owner is selected.
  test(
      'ChooseDestinationPageDifferentOwnerOnNextCallsSetDifferentOwner',
      async () => {
        await initializeChooseDestinationPage();
        const expectedPromise =
            new PromiseResolver<{stateResult: StateResult}>();
        assert(service);
        service.setDifferentOwner = () => expectedPromise.promise;
        assert(component);
        const newOwnerComponent = strictQuery(
            destinationNewOwnerSelector, component.shadowRoot,
            CrRadioButtonElement);

        newOwnerComponent.click();
        assertTrue(newOwnerComponent.checked);
        assertEquals(expectedPromise.promise, component.onNextButtonClick());
      });

  // Verify the correct function is called when not sure owner is selected.
  test(
      'ChooseDestinationPageNotSureOwnerOnNextCallsSetDifferentOwner',
      async () => {
        await initializeChooseDestinationPage();
        const expectedPromise =
            new PromiseResolver<{stateResult: StateResult}>();
        assert(service);
        service.setDifferentOwner = () => expectedPromise.promise;
        assert(component);
        const notSureOwnerComponent = strictQuery(
            destinationNotSureOwnerSelector, component.shadowRoot,
            CrRadioButtonElement);

        notSureOwnerComponent.click();
        assertTrue(notSureOwnerComponent.checked);
        assertEquals(expectedPromise.promise, component.onNextButtonClick());
      });

  // Verify the radio button group is disabled when `allButtonsDisabled` is
  // true.
  test('ChooseDestinationPageDisabledRadioGroup', async () => {
    await initializeChooseDestinationPage();
    assert(component);
    const chooseDestinationGroup = strictQuery(
        '#chooseDestinationGroup', component.shadowRoot, CrRadioGroupElement);
    assertFalse(chooseDestinationGroup.disabled);
    component.allButtonsDisabled = true;
    assertTrue(chooseDestinationGroup.disabled);
  });
});
