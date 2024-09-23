// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingWaitForManualWpDisablePage} from 'chrome://shimless-rma/onboarding_wait_for_manual_wp_disable_page.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';


suite('onboardingWaitForManualWpDisablePageTest', function() {
  let component: OnboardingWaitForManualWpDisablePage|null = null;

  let service: FakeShimlessRmaService|null = null;

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

  function initializeWaitForManualWpDisablePage(): Promise<void> {
    assert(!component);
    component = document.createElement(OnboardingWaitForManualWpDisablePage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the correct text is shown based on the current write protection
  // status.
  test('HwwpEnabledOrDisabled', async () => {
    await initializeWaitForManualWpDisablePage();

    assert(component);
    const title = strictQuery('#title', component.shadowRoot, HTMLElement);
    const manualDisableComponent = strictQuery(
        '#manuallyDisableHwwpInstructions', component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('manuallyDisableWpTitleText'),
        title.textContent!.trim());
    assertEquals(
        loadTimeData.getString('manuallyDisableWpInstructionsText'),
        manualDisableComponent.textContent!.trim());

    // Disable write protect and expect the page text to update.
    component.onHardwareWriteProtectionStateChanged(/* enabled= */ false);
    assertEquals(
        loadTimeData.getString('manuallyDisableWpTitleTextReboot'),
        title.textContent!.trim());
    assertEquals(
        loadTimeData.getString('manuallyDisableWpInstructionsTextReboot'),
        manualDisableComponent.textContent!.trim());
  });
});
