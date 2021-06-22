// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingEnterRsuWpDisableCodePageElement} from 'chrome://shimless-rma/onboarding_enter_rsu_wp_disable_code_page.js';
import {assertDeepEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingEnterRsuWpDisableCodePageTest() {
  /** @type {?OnboardingEnterRsuWpDisableCodePageElement} */
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
  function initializeEnterRsuWpDisableCodePage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingEnterRsuWpDisableCodePageElement} */ (
        document.createElement('onboarding-enter-rsu-wp-disable-code-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('EnterRsuWpDisableCodePageInitializes', async () => {
    await initializeEnterRsuWpDisableCodePage();
    const rsuCodeComponent = component.shadowRoot.querySelector('#rsuCode');
    assertFalse(rsuCodeComponent.hidden);
  });

  test(
      'EnterRsuWpDisableCodePageSetCodeOnNextCallsSetRsuDisableWriteProtectCode',
      async () => {
        const resolver = new PromiseResolver();
        await initializeEnterRsuWpDisableCodePage();
        let expectedCode = 'rsu code';
        let savedCode = '';
        service.setRsuDisableWriteProtectCode = (code) => {
          savedCode = code;
          return resolver.promise;
        };
        const rsuCodeComponent = component.shadowRoot.querySelector('#rsuCode');
        rsuCodeComponent.value = expectedCode;

        let expectedResult = {foo: 'bar'};
        let savedResult;
        component.onNextButtonClick().then((result) => savedResult = result);
        // Resolve to a distinct result to confirm it was not modified.
        resolver.resolve(expectedResult);
        await flushTasks();

        assertDeepEquals(savedCode, expectedCode);
        assertDeepEquals(savedResult, expectedResult);
      });
}
