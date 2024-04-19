// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OPEN_LOGS_DIALOG} from 'chrome://shimless-rma/events.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ShutdownMethod, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {WrapupRepairCompletePage} from 'chrome://shimless-rma/wrapup_repair_complete_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('wrapupRepairCompletePageTest', function() {
  let component: WrapupRepairCompletePage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  // ShimlessRma is needed to handle the 'transition-state' event used by the
  // shutdown and reboot buttons.
  let shimlessRmaComponent: ShimlessRma|null = null;

  const shutdownButtonSelector = '#shutDownButton';
  const rebootButtonSelector = '#rebootButton';
  const batteryCutButtonSelector = '#batteryCutButton';
  const batteryCutShutdownButtonSelector = '#batteryCutoffShutdownButton';
  const powerwashButtonSelector = '#powerwashButton';
  const rmaLogButtonSelector = '#rmaLogButton';
  const closeBatteryCutButtonSelector = '#closeBatteryCutoffDialogButton';
  const powerwashDialogSelector = '#powerwashDialog';
  const batteryCutoffDialogSelector = '#batteryCutoffDialog';

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

  function initializeRepairCompletePage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(WrapupRepairCompletePage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  function clickButton(buttonNameSelector: string): Promise<void> {
    assert(component);
    strictQuery(buttonNameSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  // Verify the page renders with the logs dialog closed.
  test('ComponentRenders', async () => {
    await initializeRepairCompletePage();

    assert(component);
    assertFalse(
        strictQuery('#logsDialog', component.shadowRoot, CrDialogElement).open);
  });

  // Verify Shimless shuts down immediately after clicking the Shutdown button
  // when a powerwash isn't required.
  test('ShutDownButtonTriggersShutDownIfNoPowerwashRequired', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      ++callCount;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };

    await clickButton(shutdownButtonSelector);

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kShutdown, shutdownMethod);
  });

  // Verify the powerwash dialog opens after clicking the Shutdown button if
  // a powerwash is required.
  test('ShutDownButtonOpensPowerwashDialogIfPowerwashRequired', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      ++callCount;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };

    await clickButton(shutdownButtonSelector);

    // Don't shut down immediately.
    assertEquals(0, callCount);
    // Show the dialog instead.
    assert(component);
    const powerwashDialog = strictQuery(
        powerwashDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(powerwashDialog.open);

    // Click powerwash button and now expect Shimless to end.
    await clickButton(powerwashButtonSelector);
    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kShutdown, shutdownMethod);
    assertFalse(powerwashDialog.open);
  });

  // Verify if the reboot button opens the powerwash dialog then a reboot is
  // triggered instead of a shutdown.
  test('PowerwashButtonTriggersRebootIfOpenedWithRebootButton', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      ++callCount;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton(rebootButtonSelector);
    await clickButton(powerwashButtonSelector);

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kReboot, shutdownMethod);
    assert(component);
    assertFalse(
        strictQuery(
            powerwashDialogSelector, component.shadowRoot, CrDialogElement)
            .open);
  });

  // Verify Shimless reboots immediately after clicking the Reboot button when a
  // powerwash isn't required.
  test('RebootButtonTriggersRebootIfNoPowerwashRequired', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      ++callCount;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };

    await clickButton(rebootButtonSelector);

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kReboot, shutdownMethod);
  });

  // Verify clicking the Battery Cut button attempts to cutoff the battery.
  test('CutoffBatteryButtonCutsOffBattery', async () => {
    await initializeRepairCompletePage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      ++callCount;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };

    assert(component);
    component.batteryTimeoutInMs = 0;
    strictQuery(batteryCutButtonSelector, component.shadowRoot, CrButtonElement)
        .disabled = false;
    await clickButton(batteryCutButtonSelector);

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kBatteryCutoff, shutdownMethod);
    // When the countdown is done, the battery cutoff dialog will be closed.
    assertFalse(
        strictQuery(
            batteryCutoffDialogSelector, component.shadowRoot, CrDialogElement)
            .open);
  });

  // Verify that plugging in the charger will cancel an active battery cutoff
  // timer.
  test('PowerCableConnectCancelsBatteryCutoff', async () => {
    await initializeRepairCompletePage();

    service.triggerPowerCableObserver(/* pluggedIn= */ false, /* delayMs= */ 0);
    await flushTasks();

    await clickButton(batteryCutButtonSelector);
    service.triggerPowerCableObserver(/* pluggedIn= */ true, /* delayMs= */ 0);
    await flushTasks();

    assert(component);
    assertEquals(-1, component.batteryTimeoutID);
  });

  // Verify clicking the Shutdown button in performs a battery cutoff.
  test('ShutdownButtonInBatteryCutoffDialogTriggersBatteryCutoff', async () => {
    await initializeRepairCompletePage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      ++callCount;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };

    // Force the battery cutoff dialog to open, to make sure that the shutdown
    // button closes it.
    assert(component);
    const batteryCutoffDialog = strictQuery(
        batteryCutoffDialogSelector, component.shadowRoot, CrDialogElement);
    batteryCutoffDialog.showModal();
    assertTrue(batteryCutoffDialog.open);

    await clickButton(batteryCutShutdownButtonSelector);
    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kBatteryCutoff, shutdownMethod);
    assertFalse(batteryCutoffDialog.open);
  });

  // Verify clicking the log button sends the trigger for the logs dialog to
  // open.
  test('OpensRmaLogDialog', async () => {
    await initializeRepairCompletePage();

    assert(component);
    const openLogsEvent = eventToPromise(OPEN_LOGS_DIALOG, component);

    await clickButton(rmaLogButtonSelector);
    await openLogsEvent;
  });

  // Verify the battery cutoff button starts disabled by default.
  test('BatteryCutButtonDisabledByDefault', async () => {
    await initializeRepairCompletePage();

    assert(component);
    assertTrue(
        strictQuery(
            batteryCutButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
  });

  // Verify unplugging the power cable enables the battery cutoff button.
  test('PowerCableStateFalseEnablesBatteryCutButton', async () => {
    await initializeRepairCompletePage();

    service.triggerPowerCableObserver(/* pluggedIn= */ false, /* delayMs= */ 0);
    await flushTasks();

    assert(component);
    assertFalse(
        strictQuery(
            batteryCutButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
  });

  // Verify clicking the cancel button closes the powerwash dialog.
  test('PowerwashCancelButtonClosesPowerwashDialog', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    await clickButton(shutdownButtonSelector);

    assert(component);
    const powerwashDialog = strictQuery(
        powerwashDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(powerwashDialog.open);
    await clickButton('#closePowerwashDialogButton');
    assertFalse(powerwashDialog.open);
  });

  // Verify clicking the cancel button cancels battery cutoff and closes the
  // dialog.
  test('CutoffCanceButtonClosesCutoffDialog', async () => {
    await initializeRepairCompletePage();

    assert(component);
    strictQuery(batteryCutButtonSelector, component.shadowRoot, CrButtonElement)
        .disabled = false;
    await clickButton(batteryCutButtonSelector);

    assert(component);
    const batteryCutoffDialog = strictQuery(
        batteryCutoffDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(batteryCutoffDialog.open);

    await clickButton(closeBatteryCutButtonSelector);
    assertFalse(batteryCutoffDialog.open);
    assertEquals(-1, component.batteryTimeoutID);
  });


  // Verify when `allButtonsDisabled` is set all inputs are disabled
  test('AllButtonsDisabled', async () => {
    await initializeRepairCompletePage();

    assert(component);
    const shutDownButton = strictQuery(
        shutdownButtonSelector, component.shadowRoot, CrButtonElement);
    const rebootButton = strictQuery(
        rebootButtonSelector, component.shadowRoot, CrButtonElement);
    const diagnosticsButton = strictQuery(
        '#diagnosticsButton', component.shadowRoot, CrButtonElement);
    const rmaLogButton = strictQuery(
        rmaLogButtonSelector, component.shadowRoot, CrButtonElement);
    const batteryCutButton = strictQuery(
        batteryCutButtonSelector, component.shadowRoot, CrButtonElement);

    component.allButtonsDisabled = false;
    assertFalse(shutDownButton.disabled);
    assertFalse(rebootButton.disabled);
    assertFalse(diagnosticsButton.disabled);
    assertFalse(rmaLogButton.disabled);

    service.triggerPowerCableObserver(false, 0);
    await flushTasks();
    assertFalse(batteryCutButton.disabled);

    component.allButtonsDisabled = true;
    assertTrue(shutDownButton.disabled);
    assertTrue(rebootButton.disabled);
    assertTrue(diagnosticsButton.disabled);
    assertTrue(rmaLogButton.disabled);
    assertTrue(batteryCutButton.disabled);
  });

  // Verify the shutdown button stays disabled after a shutdown is initiated
  // and still in progress.
  test('ShutdownButtonsStayDisabledAfterShutdown', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    service.endRma = () => {
      return resolver.promise;
    };

    await clickButton(shutdownButtonSelector);

    assert(component);
    assertTrue(
        strictQuery(
            shutdownButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
    assertTrue(
        strictQuery(rebootButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
  });

  test('ShutdownButtonsStayDisabledAfterReboot', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    service.endRma = () => {
      return resolver.promise;
    };

    await clickButton(rebootButtonSelector);

    assert(component);
    assertTrue(
        strictQuery(
            shutdownButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
    assertTrue(
        strictQuery(rebootButtonSelector, component.shadowRoot, CrButtonElement)
            .disabled);
  });
});
