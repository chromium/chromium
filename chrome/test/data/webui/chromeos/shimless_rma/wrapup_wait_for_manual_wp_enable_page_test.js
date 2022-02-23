// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {WrapupWaitForManualWpEnablePage} from 'chrome://shimless-rma/wrapup_wait_for_manual_wp_enable_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function wrapupWaitForManualWpEnablePageTest() {
  /** @type {?WrapupWaitForManualWpEnablePage} */
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
  function initializeWaitForManualWpEnablePage() {
    assertFalse(!!component);

    component = /** @type {!WrapupWaitForManualWpEnablePage} */ (
        document.createElement('wrapup-wait-for-manual-wp-enable-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('WaitForManualWpEnablePageInitializes', async () => {
    await initializeWaitForManualWpEnablePage();
    const manualEnableComponent =
        component.shadowRoot.querySelector('#manuallyEnableHwwpInstructions');
    assertFalse(manualEnableComponent.hidden);
  });

  test('HwwpDisabledDisablesNext', async () => {
    await initializeWaitForManualWpEnablePage();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(
        savedError.message, 'Hardware Write Protection is not enabled.');
    assertEquals(savedResult, undefined);
  });

  test('HwwpEnabledEnablesNext', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForManualWpEnablePage();
    service.triggerHardwareWriteProtectionObserver(true, 0);
    await flushTasks();
    service.writeProtectManuallyEnabled = () => {
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertDeepEquals(savedResult, expectedResult);
  });
}
