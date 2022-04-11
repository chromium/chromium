// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ShutdownMethod} from 'chrome://shimless-rma/shimless_rma_types.js';
import {WrapupRepairCompletePage} from 'chrome://shimless-rma/wrapup_repair_complete_page.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function wrapupRepairCompletePageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used by
   * the rework button.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?WrapupRepairCompletePage} */
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
    shimless_rma_component.remove();
    shimless_rma_component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeRepairCompletePage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

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

  test('CanCutOffBattery', async () => {
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

    const cutButton = component.shadowRoot.querySelector('#batteryCutButton');
    cutButton.disabled = false;

    await clickButton('#batteryCutButton');
    await flushTasks();

    assertEquals(1, callCount);
    assertEquals(ShutdownMethod.kBatteryCutoff, shutdownMethod);
  });

  test('OpensRmaLogDialog', async () => {
    await initializeRepairCompletePage();
    await clickButton('#rmaLogButton');

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertTrue(logsDialog.open);
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

  test('DialogCloses', async () => {
    await initializeRepairCompletePage();
    await clickButton('#rmaLogButton');
    await clickButton('#closeLogDialogButton');

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);

    await clickButton('#batteryCutButton');
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
}
