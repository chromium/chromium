// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {RmadErrorCode, ShutdownMethod} from 'chrome://shimless-rma/shimless_rma_types.js';
import {WrapupRepairCompletePage} from 'chrome://shimless-rma/wrapup_repair_complete_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

suite('wrapupRepairCompletePageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used by
   * the rework button.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?WrapupRepairCompletePage} */
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
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeRepairCompletePage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!WrapupRepairCompletePage} */ (
        document.createElement('wrapup-repair-complete-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @param {string} buttonNameSelector
   * @return {!Promise}
   */
  function clickButton(buttonNameSelector) {
    assertTrue(!!component);

    const button = component.shadowRoot.querySelector(buttonNameSelector);
    button.click();
    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeRepairCompletePage();
    assertTrue(!!component);

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);
  });

  test('ShutDownButtonTriggersShutDownIfNoPowerwashRequired', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#shutDownButton');
    await flushTasks();

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kShutdown, shutdownMethod);
  });

  test('ShutDownButtonOpensPowerwashDialogIfPowerwashRequired', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    let callCount = 0;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#shutDownButton');
    await flushTasks();

    // Don't shut down immediately.
    assertEquals(0, callCount);
    // Show the dialog instead.
    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);
    assertTrue(powerwashDialog.open);
  });

  test(
      'PowerwashButtonTriggersShutDownIfOpenedWithShutdownButton', async () => {
        const resolver = new PromiseResolver();
        await initializeRepairCompletePage();

        service.setGetPowerwashRequiredResult(true);

        let callCount = 0;
        let shutdownMethod;
        service.endRma = (seenShutdownMethod) => {
          callCount++;
          shutdownMethod = seenShutdownMethod;
          return resolver.promise;
        };
        await flushTasks();

        await clickButton('#shutDownButton');
        await clickButton('#powerwashButton');
        await flushTasks();

        assertEquals(1, callCount);
        assertEquals(ShutdownMethod.kShutdown, shutdownMethod);
      });

  test('PowerwashButtonTriggersRebootIfOpenedWithRebootButton', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#rebootButton');
    await clickButton('#powerwashButton');
    await flushTasks();

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kReboot, shutdownMethod);
  });

  test('PowerwashDialogClosesWhenCompletedWithShutdown', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    await clickButton('#shutDownButton');
    await clickButton('#powerwashButton');
    await flushTasks();

    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);
    assertFalse(powerwashDialog.open);
  });

  test('PowerwashDialogClosesWhenCompletedWithReboot', async () => {
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    await clickButton('#rebootButton');
    await clickButton('#powerwashButton');
    await flushTasks();

    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);
    assertFalse(powerwashDialog.open);
  });

  test('RebootButtonTriggersRebootIfNoPowerwashRequired', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#rebootButton');
    await flushTasks();

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kReboot, shutdownMethod);
  });

  test('RebootButtonOpensPowerwashDialogIfPowerwashRequired', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(true);

    let callCount = 0;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#rebootButton');
    await flushTasks();

    // Don't reboot immediately.
    assertEquals(0, callCount);
    // Show the dialog instead.
    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);
    assertTrue(powerwashDialog.open);
  });

  test('CutoffBatteryButtonCutsOffBattery', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();
    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };
    await flushTasks();

    component.batteryTimeoutInMs_ = 0;

    const cutButton = component.shadowRoot.querySelector('#batteryCutButton');
    cutButton.disabled = false;

    await clickButton('#batteryCutButton');
    await flushTasks();

    // Cut off the battery.
    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kBatteryCutoff, shutdownMethod);
    // When the countdown is done, the battery cutoff dialog will be closed.
    const batteryCutoffDialog =
        component.shadowRoot.querySelector('#batteryCutoffDialog');
    assertTrue(!!batteryCutoffDialog);
    assertFalse(batteryCutoffDialog.open);
  });

  test('PowerCableConnectCancelsBatteryCutoff', async () => {
    await initializeRepairCompletePage();
    await flushTasks();

    service.triggerPowerCableObserver(false, 0);
    await flushTasks();

    await clickButton('#batteryCutButton');
    service.triggerPowerCableObserver(true, 0);
    await flushTasks();

    assertEquals(-1, component.batteryTimeoutID_);
  });

  test('ShutdownButtonInBatteryCutoffDialogTriggersBatteryCutoff', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    let callCount = 0;
    let shutdownMethod;
    service.endRma = (seenShutdownMethod) => {
      callCount++;
      shutdownMethod = seenShutdownMethod;
      return resolver.promise;
    };
    await flushTasks();

    // Force the battery cutoff dialog to open, to make sure that the shutdown
    // button closes it.
    const batteryCutoffDialog =
        component.shadowRoot.querySelector('#batteryCutoffDialog');
    assertTrue(!!batteryCutoffDialog);
    batteryCutoffDialog.showModal();
    assertTrue(batteryCutoffDialog.open);

    await clickButton('#batteryCutoffShutdownButton');
    await flushTasks();

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kBatteryCutoff, shutdownMethod);
    assertFalse(batteryCutoffDialog.open);
  });

  test('OpensRmaLogDialog', async () => {
    await initializeRepairCompletePage();

    let openLogsDialogEventFired = false;
    const eventHandler = (event) => {
      openLogsDialogEventFired = true;
    };
    component.addEventListener('open-logs-dialog', eventHandler);

    await clickButton('#rmaLogButton');

    assertTrue(openLogsDialogEventFired);
  });

  test('BatteryCutButtonDisabledByDefault', async () => {
    await initializeRepairCompletePage();
    const button = component.shadowRoot.querySelector('#batteryCutButton');

    assertTrue(!!button);
    assertTrue(button.disabled);
  });

  test('PowerCableStateTrueDisablesBatteryCutButton', async () => {
    await initializeRepairCompletePage();
    service.triggerPowerCableObserver(true, 0);
    await flushTasks();
    const button = component.shadowRoot.querySelector('#batteryCutButton');

    assertTrue(!!button);
    assertTrue(button.disabled);
  });

  test('PowerCableStateFalseEnablesBatteryCutButton', async () => {
    await initializeRepairCompletePage();
    service.triggerPowerCableObserver(false, 0);
    await flushTasks();
    const button = component.shadowRoot.querySelector('#batteryCutButton');

    assertTrue(!!button);
    assertFalse(button.disabled);
  });

  test('PowerwashCancelButtonClosesPowerwashDialog', async () => {
    await initializeRepairCompletePage();
    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);

    service.setGetPowerwashRequiredResult(true);

    await clickButton('#shutDownButton');
    assertTrue(powerwashDialog.open);

    await clickButton('#closePowerwashDialogButton');
    assertFalse(powerwashDialog.open);
  });

  test('CutoffCancelButtonClosesCutoffDialog', async () => {
    await initializeRepairCompletePage();
    const batteryCutoffDialog =
        component.shadowRoot.querySelector('#batteryCutoffDialog');
    assertTrue(!!batteryCutoffDialog);

    const batteryCutButton =
        component.shadowRoot.querySelector('#batteryCutButton');
    batteryCutButton.disabled = false;

    await clickButton('#batteryCutButton');
    assertTrue(batteryCutoffDialog.open);

    await clickButton('#closeBatteryCutoffDialogButton');
    assertFalse(batteryCutoffDialog.open);
  });

  test('CutoffCancelButtonCancelsBatteryCutoff', async () => {
    await initializeRepairCompletePage();
    await flushTasks();

    const batteryCutButton =
        component.shadowRoot.querySelector('#batteryCutButton');
    batteryCutButton.disabled = false;

    await clickButton('#batteryCutButton');

    await clickButton('#closeBatteryCutoffDialogButton');
    assertEquals(-1, component.batteryTimeoutID_);
  });

  test('AllButtonsDisabled', async () => {
    await initializeRepairCompletePage();
    const shutDownButton =
        component.shadowRoot.querySelector('#shutDownButton');
    const rebootButton = component.shadowRoot.querySelector('#rebootButton');
    const diagnosticsButton =
        component.shadowRoot.querySelector('#diagnosticsButton');
    const rmaLogButton = component.shadowRoot.querySelector('#rmaLogButton');
    const batteryCutButton =
        component.shadowRoot.querySelector('#batteryCutButton');

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

  test('ShutdownButtonsStayDisabledAfterShutdown', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    service.endRma = (seenShutdownMethod) => {
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#shutDownButton');
    await flushTasks();

    assertTrue(component.shadowRoot.querySelector('#shutDownButton').disabled);
    assertTrue(component.shadowRoot.querySelector('#rebootButton').disabled);
  });

  test('ShutdownButtonsStayDisabledAfterReboot', async () => {
    const resolver = new PromiseResolver();
    await initializeRepairCompletePage();

    service.setGetPowerwashRequiredResult(false);

    service.endRma = (seenShutdownMethod) => {
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#rebootButton');
    await flushTasks();

    assertTrue(component.shadowRoot.querySelector('#shutDownButton').disabled);
    assertTrue(component.shadowRoot.querySelector('#rebootButton').disabled);
  });
});
