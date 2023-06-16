// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseDestinationPageElement} from 'chrome://shimless-rma/onboarding_choose_destination_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('onboardingChooseDestinationPageTest', function() {
  /** @type {?OnboardingChooseDestinationPageElement} */
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
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeChooseDestinationPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingChooseDestinationPageElement} */ (
        document.createElement('onboarding-choose-destination-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ChooseDestinationPageInitializes', async () => {
    await initializeChooseDestinationPage();
    const originalOwnerComponent =
        component.shadowRoot.querySelector('#destinationOriginalOwner');
    const newOwnerComponent =
        component.shadowRoot.querySelector('#destinationNewOwner');

    assertFalse(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);
  });

  test('ChooseDestinationPageOneChoiceOnly', async () => {
    await initializeChooseDestinationPage();
    const originalOwnerComponent =
        component.shadowRoot.querySelector('#destinationOriginalOwner');
    const newOwnerComponent =
        component.shadowRoot.querySelector('#destinationNewOwner');
    const notSureOwnerComponent =
        component.shadowRoot.querySelector('#destinationNotSureOwner');

    originalOwnerComponent.click();
    await flushTasks;

    assertTrue(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);
    assertFalse(notSureOwnerComponent.checked);

    newOwnerComponent.click();
    await flushTasks;

    assertFalse(originalOwnerComponent.checked);
    assertTrue(newOwnerComponent.checked);
    assertFalse(notSureOwnerComponent.checked);

    notSureOwnerComponent.click();
    await flushTasks;

    assertFalse(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);
    assertTrue(notSureOwnerComponent.checked);
  });

  test('ChooseDestinationPageSameOwnerOnNextCallsSetSameOwner', async () => {
    const resolver = new PromiseResolver();
    await initializeChooseDestinationPage();
    service.setSameOwner = () => resolver.promise;
    const originalOwnerComponent =
        component.shadowRoot.querySelector('#destinationOriginalOwner');

    originalOwnerComponent.click();
    await flushTasks;
    assertTrue(originalOwnerComponent.checked);

    assertEquals(component.onNextButtonClick(), resolver.promise);
  });

  test(
      'ChooseDestinationPageDifferentOwnerOnNextCallsSetDifferentOwner',
      async () => {
        const resolver = new PromiseResolver();
        await initializeChooseDestinationPage();
        service.setDifferentOwner = () => resolver.promise;
        const newOwnerComponent =
            component.shadowRoot.querySelector('#destinationNewOwner');

        newOwnerComponent.click();
        await flushTasks;
        assertTrue(newOwnerComponent.checked);

        assertEquals(component.onNextButtonClick(), resolver.promise);
      });

  test(
      'ChooseDestinationPageNotSureOwnerOnNextCallsSetDifferentOwner',
      async () => {
        const resolver = new PromiseResolver();
        await initializeChooseDestinationPage();
        service.setDifferentOwner = () => resolver.promise;
        const notSureOwnerComponent =
            component.shadowRoot.querySelector('#destinationNotSureOwner');

        notSureOwnerComponent.click();
        await flushTasks;
        assertTrue(notSureOwnerComponent.checked);

        assertEquals(component.onNextButtonClick(), resolver.promise);
      });

  test('ChooseDestinationPageDisabledRadioGroup', async () => {
    await initializeChooseDestinationPage();

    const chooseDestinationGroup =
        component.shadowRoot.querySelector('#chooseDestinationGroup');
    assertFalse(chooseDestinationGroup.disabled);
    component.allButtonsDisabled = true;
    assertTrue(chooseDestinationGroup.disabled);
  });
});
