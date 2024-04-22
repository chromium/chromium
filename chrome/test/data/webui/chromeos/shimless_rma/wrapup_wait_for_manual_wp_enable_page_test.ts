// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {WrapupWaitForManualWpEnablePage} from 'chrome://shimless-rma/wrapup_wait_for_manual_wp_enable_page.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('wrapupWaitForManualWpEnablePageTest', function() {
  let component: WrapupWaitForManualWpEnablePage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  // ShimlessRma is needed to handle the 'transition-state' event.
  let shimlessRmaComponent: ShimlessRma|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  function initializeWaitForManualWpEnablePage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(WrapupWaitForManualWpEnablePage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the component initializes with instructions.
  test('WaitForManualWpEnablePageInitializes', async () => {
    await initializeWaitForManualWpEnablePage();

    assert(component);
    assertFalse(strictQuery(
                    '#manuallyEnableHwwpInstructions', component.shadowRoot,
                    HTMLElement)
                    .hidden);
  });

  // Verify the page auto transitions if WP is detected as enabled.
  test('WriteProtectEnabledAutoTransitions', async () => {
    await initializeWaitForManualWpEnablePage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    service.writeProtectManuallyEnabled = () => {
      ++callCount;
      return resolver.promise;
    };

    service.triggerHardwareWriteProtectionObserver(
        /* enabled= */ true, /* delayMs= */ 0);
    await flushTasks();
    assertEquals(1, callCount);
  });
});
