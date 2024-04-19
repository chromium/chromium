// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FATAL_HARDWARE_ERROR} from 'chrome://shimless-rma/events.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {PROVISIONING_ERROR_CODE_PREFIX, ReimagingProvisioningPage} from 'chrome://shimless-rma/reimaging_provisioning_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ProvisioningError, ProvisioningStatus, RmadErrorCode, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('reimagingProvisioningPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   */
  let shimlessRmaComponent: ShimlessRma|null = null;

  let component: ReimagingProvisioningPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  const wpEnabledDialogSelector = '#wpEnabledDialog';
  const tryAgainButtonSelector = '#tryAgainButton';

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

  function initializeWaitForProvisioningPage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(ReimagingProvisioningPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify when provisioning progress reaches 100%, the complete action is
  // called.
  test('ProvisioningCompleteTransitionsState', async () => {
    await initializeWaitForProvisioningPage();

    let provisioningComplete = false;
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    service.provisioningComplete = () => {
      provisioningComplete = true;
      return resolver.promise;
    };

    service.triggerProvisioningObserver(
        ProvisioningStatus.kComplete, /* progress= */ 1.0,
        ProvisioningError.kUnknown,
        /* delayMs= */ 0);
    await flushTasks();

    assertTrue(provisioningComplete);
  });

  // Verify the error dialog shows for a WP error and provisioning can be
  // retried.
  test('ProvisioningFailedWpError', async () => {
    await initializeWaitForProvisioningPage();

    let callCount = 0;
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    service.retryProvisioning = () => {
      ++callCount;
      return resolver.promise;
    };

    // Confirm the dialog is closed, trigger a failure then confirm the dialog
    // opens.
    assert(component);
    const wpEnabledDialog = strictQuery(
        wpEnabledDialogSelector, component.shadowRoot, CrDialogElement);
    assertFalse(wpEnabledDialog.open);
    service.triggerProvisioningObserver(
        ProvisioningStatus.kFailedBlocking, /* progress= */ 1.0,
        ProvisioningError.kWpEnabled,
        /* delayMs= */ 0);
    await flushTasks();
    assertTrue(wpEnabledDialog.open);

    // Click the try again button.
    strictQuery(tryAgainButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    assertFalse(wpEnabledDialog.open);
    assertEquals(1, callCount);
  });

  // Verify the the fatal hardware error dialog shows for a non-WP error.
  test('ProvisioningFailedNonWpError', async () => {
    await initializeWaitForProvisioningPage();

    // Trigger a blocking error.
    assert(component);
    const fatalHardwareErrorEvent =
        eventToPromise(FATAL_HARDWARE_ERROR, component);
    const expectedProvisoningError = ProvisioningError.kInternal;
    service.triggerProvisioningObserver(
        ProvisioningStatus.kFailedBlocking, 1.0, expectedProvisoningError, 0);
    await flushTasks();

    // Confirm the correct error code is displayed.
    const eventResponse = await fatalHardwareErrorEvent;
    assertEquals(
        RmadErrorCode.kProvisioningFailed, eventResponse.detail.rmadErrorCode);
    assertEquals(
        PROVISIONING_ERROR_CODE_PREFIX + expectedProvisoningError,
        eventResponse.detail.fatalErrorCode);
    assertFalse(
        strictQuery(
            wpEnabledDialogSelector, component.shadowRoot, CrDialogElement)
            .open);
  });
});
