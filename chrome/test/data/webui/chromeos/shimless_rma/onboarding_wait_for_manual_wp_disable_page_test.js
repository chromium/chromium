// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {fakeRsuChallengeQrCode} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingWaitForManualWpDisablePage} from 'chrome://shimless-rma/onboarding_wait_for_manual_wp_disable_page.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

/**
 * It is not possible to suppress visibility inline so this helper
 * function wraps the access to canvasSize_.
 * @suppress {visibility}
 */
function suppressedComponentCanvasSize_(component) {
  return component.canvasSize_;
}

export function onboardingWaitForManualWpDisablePageTest() {
  /** @type {?OnboardingWaitForManualWpDisablePage} */
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
  function initializeWaitForManualWpDisablePage() {
    service.setGetWriteProtectManuallyDisabledInstructionsResult(
        'g.co/help', fakeRsuChallengeQrCode);

    assertFalse(!!component);

    component = /** @type {!OnboardingWaitForManualWpDisablePage} */ (
        document.createElement('onboarding-wait-for-manual-wp-disable-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('WaitForManualWpDisablePageInitializes', async () => {
    await initializeWaitForManualWpDisablePage();
    const manualDisableComponent =
        component.shadowRoot.querySelector('#manuallyDisableHwwpInstructions');
    assertFalse(manualDisableComponent.hidden);
  });
}
