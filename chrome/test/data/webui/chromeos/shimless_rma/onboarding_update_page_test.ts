// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {DISABLE_ALL_BUTTONS, ENABLE_ALL_BUTTONS} from 'chrome://shimless-rma/events.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingUpdatePageElement} from 'chrome://shimless-rma/onboarding_update_page.js';
import {OsUpdateOperation, UpdateErrorCode} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('onboardingUpdatePageTest', function() {
  let component: OnboardingUpdatePageElement|null = null;

  const service: FakeShimlessRmaService|null = new FakeShimlessRmaService();

  const performUpdateButtonSelector = '#performUpdateButton';
  const versionInfoSelector = '#versionInfo';
  const updateInstructionsSelector = '#updateInstructionsDiv';
  const updateStatusSelector = '#updateStatusDiv';
  const updateErrorSelector = '#updateErrorDiv';
  const unqualifiedComponentsLinkSelector = '#unqualifiedComponentsLink';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assert(service);
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function initializeUpdatePage(version: string = ''): Promise<void> {
    // Initialize the fake data.
    assert(service);
    service.setGetCurrentOsVersionResult(version);
    service.setCheckForOsUpdatesResult(version);
    service.setUpdateOsResult(/* started= */ true);

    assert(!component);
    component = document.createElement(OnboardingUpdatePageElement.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  function clickPerformUpdateButton(): Promise<void> {
    assert(component);
    strictQuery(
        performUpdateButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  // Verify the pages initializes with the text and components in the expected
  // state.
  test('PageInitializes', async () => {
    const version = '90.1.2.3';
    await initializeUpdatePage(version);

    assert(component);
    assertEquals(
        loadTimeData.getStringF('currentVersionOutOfDateText', version),
        strictQuery(versionInfoSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());
    assertFalse(
        strictQuery(
            performUpdateButtonSelector, component.shadowRoot, CrButtonElement)
            .hidden);
  });

  // Verify clicking the update button starts then disables all buttons.
  test('UpdateStarts', async () => {
    await initializeUpdatePage();

    assert(component);
    const disableAllButtonsEvent =
        eventToPromise(DISABLE_ALL_BUTTONS, component);
    await clickPerformUpdateButton();
    await disableAllButtonsEvent;

    component.allButtonsDisabled = true;
    assertTrue(
        strictQuery(
            performUpdateButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
  });

  // Verify the update instructions show then progress updates are visible.
  test('ShowsUpdateProgress', async () => {
    await initializeUpdatePage();

    assert(component);
    const updateInstructionsDiv = strictQuery(
        updateInstructionsSelector, component.shadowRoot, HTMLElement);
    const updateStatusDiv =
        strictQuery(updateStatusSelector, component.shadowRoot, HTMLElement);
    const updateErrorDiv =
        strictQuery(updateErrorSelector, component.shadowRoot, HTMLElement);
    assertFalse(updateInstructionsDiv.hidden);
    assertTrue(updateStatusDiv.hidden);
    assertTrue(updateErrorDiv.hidden);

    await clickPerformUpdateButton();

    assert(service);
    service.triggerOsUpdateObserver(
        OsUpdateOperation.kDownloading, /* progress= */ 0.5,
        UpdateErrorCode.kSuccess, /* delayMs= */ 0);
    await flushTasks();

    assertTrue(updateInstructionsDiv.hidden);
    assertFalse(updateStatusDiv.hidden);
    assertTrue(updateErrorDiv.hidden);
  });

  // Verify the unqualified link is hidden when the hardware verifier returns
  // all components are compliant.
  test('ShowHideUnqualifiedComponentsLink', async () => {
    await initializeUpdatePage();

    // Simulate all compliant hardware.
    assert(service);
    service.triggerHardwareVerificationStatusObserver(
        /* isCompliant= */ true, /* errorMessage= */ '', /* delayMs= */ 0);
    await flushTasks();

    // Verify the unqualified link isn't showing.
    assert(component);
    assert(!component.shadowRoot!.querySelector(
        unqualifiedComponentsLinkSelector));
  });

  // Verify the unqualified link can be clicked and opens the dialog when the
  // hardware verifier a non-compliant component.
  test('ShowLinkOpenDialogOnError', async () => {
    await initializeUpdatePage();

    const failedComponent = 'Keyboard';
    assert(service);
    service.triggerHardwareVerificationStatusObserver(
        /* isCompliant= */ false, /* errorMessage= */ failedComponent,
        /* delayMs= */ 0);
    await flushTasks();

    assert(component);
    const unqualifiedComponentsLink = strictQuery(
        unqualifiedComponentsLinkSelector, component.shadowRoot, HTMLElement);
    assertTrue(isVisible(unqualifiedComponentsLink));
    unqualifiedComponentsLink.click();

    assertTrue(strictQuery(
                   '#unqualifiedComponentsDialog', component.shadowRoot,
                   CrDialogElement)
                   .open);
    assertEquals(
        failedComponent,
        strictQuery('#dialogBody', component.shadowRoot, HTMLElement)
            .textContent!.trim());
  });

  // Verify an error shows when an update fails.
  test('ShowsErrorsOnFailedUpdate', async () => {
    await initializeUpdatePage();

    assert(component);
    const updateInstructionsDiv = strictQuery(
        updateInstructionsSelector, component.shadowRoot, HTMLElement);
    const updateStatusDiv =
        strictQuery(updateStatusSelector, component.shadowRoot, HTMLElement);
    const updateErrorDiv =
        strictQuery(updateErrorSelector, component.shadowRoot, HTMLElement);
    assertFalse(updateInstructionsDiv.hidden);
    assertTrue(updateStatusDiv.hidden);
    assertTrue(updateErrorDiv.hidden);

    await clickPerformUpdateButton();

    // Trigger a failed update.
    assert(service);
    service.triggerOsUpdateObserver(
        OsUpdateOperation.kReportingErrorEvent, /* progress= */ 0.5,
        UpdateErrorCode.kDownloadError, /* delayMs= */ 0);
    await flushTasks();

    // Only the error message should show.
    assertTrue(updateInstructionsDiv.hidden);
    assertTrue(updateStatusDiv.hidden);
    assertFalse(updateErrorDiv.hidden);
  });

  // Verify an update can be retried after a failure.
  test('AllowRetryAfterError', async () => {
    await initializeUpdatePage();

    await clickPerformUpdateButton();

    assert(service);
    service.triggerOsUpdateObserver(
        OsUpdateOperation.kReportingErrorEvent, /* progress= */ 0.5,
        UpdateErrorCode.kDownloadError, /* delayMs= */ 0);
    await flushTasks();

    // The error should show after the failed update.
    assert(component);
    const updateErrorDiv =
        strictQuery(updateErrorSelector, component.shadowRoot, HTMLElement);
    assertFalse(updateErrorDiv.hidden);

    // Initiate a retry.
    strictQuery('#retryUpdateButton', component.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    // This should send us back to the update progress screen.
    assertTrue(updateErrorDiv.hidden);
  });

  // Verify all buttons are enabled after a failed update attempt.
  test('UpdateFailedToStartButtonsEnabled', async () => {
    await initializeUpdatePage();

    assert(service);
    service.setUpdateOsResult(/* started= */ false);

    assert(component);
    const enableAllButtonsEvent = eventToPromise(ENABLE_ALL_BUTTONS, component);
    await clickPerformUpdateButton();
    await enableAllButtonsEvent;
  });
});
