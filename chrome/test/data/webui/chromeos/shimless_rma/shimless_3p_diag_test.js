// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {Shimless3pDiagnostics} from 'chrome://shimless-rma/shimless_3p_diagnostics.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {Show3pDiagnosticsAppResult} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise} from '../test_util.js';

suite('shimless3pDiagTest', function() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?Shimless3pDiagnostics} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  /**
   * Used to verify that all buttons has been disabled. This is to check if
   * users see the UI changes.
   * @type {boolean}
   * */
  let hasDisabledAllButtons = false;

  /**
   * The current disabled state of buttons.
   * @type {boolean}
   * */
  let isAllButtonsDisabled = false;

  /**@type function() */
  const disableAllButtonsListener = () => {
    hasDisabledAllButtons = true;
    isAllButtonsDisabled = true;
  };
  /**@type function() */
  const enableAllButtonsListener = () => {
    isAllButtonsDisabled = false;
  };

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
    window.addEventListener('disable-all-buttons', disableAllButtonsListener);
    window.addEventListener('enable-all-buttons', enableAllButtonsListener);
    hasDisabledAllButtons = false;
    isAllButtonsDisabled = false;

    loadTimeData.overrideValues({'3pDiagnosticsEnabled': true});
    service.setGet3pDiagnosticsProviderResult('Google');
    service.setInstallable3pDiagnosticsAppPath(null);
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kAppNotInstalled);
  });

  teardown(() => {
    window.removeEventListener('enable-all-buttons', enableAllButtonsListener);
    window.removeEventListener(
        'disable-all-buttons', disableAllButtonsListener);
    component.remove();
    component = null;
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  /**@type function(): !Promise */
  const initialize = () => {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!Shimless3pDiagnostics} */ (
        shimlessRmaComponent.shadowRoot.querySelector(
            '#shimless3pDiagnostics'));
    assertTrue(!!component);

    return flushTasks();
  };

  /**@type function(string): boolean */
  const isDialogOpen = (selector) => {
    assertTrue(!!component);

    const dialog = component.shadowRoot.querySelector(selector);
    assertTrue(!!dialog);
    return dialog.open;
  };

  /**@type function(string): !Promise */
  const clickButton = (selector) => {
    assertTrue(!!component);

    const button = component.shadowRoot.querySelector(selector);
    button.click();
    return flushTasks();
  };

  /**@type function(string, boolean, boolean): !Promise */
  const pressKey = (key, altKey, shiftKey) => {
    const eventPromise = eventToPromise('keydown', component);
    component.dispatchEvent(new KeyboardEvent(
        'keydown',
        {
          bubbles: true,
          composed: true,
          key,
          altKey,
          shiftKey,
        },
        ));
    return eventPromise;
  };

  /**@type function(string): !Promise */
  const pressEnterOnDialog = (selector) => {
    const dialog = component.shadowRoot.querySelector(selector);
    assertTrue(!!dialog);
    const eventPromise = eventToPromise('keypress', dialog);
    dialog.dispatchEvent(new KeyboardEvent(
        'keypress',
        {
          bubbles: true,
          composed: true,
          key: 'Enter',
        },
        ));
    return eventPromise;
  };

  /**@type function(string): !Promise */
  const cancelDialog = (selector) => {
    const dialog = component.shadowRoot.querySelector(selector);
    assertTrue(!!dialog);
    const eventPromise = eventToPromise('cancel', component);
    dialog.getNative().dispatchEvent(
        new CustomEvent('cancel', {cancelable: true}));
    return eventPromise;
  };

  // Test initialization of 3p diag.
  test('initialize', async () => {
    await initialize();
    assertTrue(!!component);
  });

  // Verify 3p diag is disabled by flag.
  test('3pDiagIsDisabledByFlag', async () => {
    loadTimeData.overrideValues({'3pDiagnosticsEnabled': false});
    await initialize();
    assertTrue(!!component);

    component.launch3pDiagnostics();

    await flushTasks();
    assertFalse(hasDisabledAllButtons);
  });

  // If provider is not yet fetched, should not trigger the launch.
  test('ProviderIsNotYetFetched', async () => {
    // Set a delay to simulate the provider is not yet fetched. We don't
    // actually wait this to be done.
    service.setAsyncOperationDelayMs(1000);
    await initialize();
    assertTrue(!!component);

    component.launch3pDiagnostics();

    await flushTasks();
    assertFalse(hasDisabledAllButtons);
  });

  // If no provider, should not trigger the launch.
  test('NoProvider', async () => {
    service.setGet3pDiagnosticsProviderResult(null);
    await initialize();
    assertTrue(!!component);

    component.launch3pDiagnostics();

    await flushTasks();
    assertFalse(hasDisabledAllButtons);
  });

  // Verify the flow that users trigger 3p diag when there is no installed app.
  test('AppNotInstall', async () => {
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kAppNotInstalled);
    await initialize();
    assertTrue(!!component);

    component.launch3pDiagnostics();

    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertEquals(
        'Google diagnostics app is not installed',
        component.shadowRoot.querySelector('#shimless3pDiagErrorDialogTitle')
            .textContent.trim());
    assertEquals(
        'Check with the device manufacturer',
        component.shadowRoot.querySelector('#shimless3pDiagErrorDialogBody')
            .textContent.trim());

    await clickButton('#shimless3pDiagErrorDialogButton');
    assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertFalse(isAllButtonsDisabled);
  });

  // Verify the flow that users trigger 3p diag, there is an installed app, but
  // we fail to load the app.
  test('AppFailedToLoad', async () => {
    service.setShow3pDiagnosticsAppResult(
        Show3pDiagnosticsAppResult.kFailedToLoad);
    await initialize();
    assertTrue(!!component);

    component.launch3pDiagnostics();

    await flushTasks();
    assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertEquals(
        'Couldn\'t load Google diagnostics app',
        component.shadowRoot.querySelector('#shimless3pDiagErrorDialogTitle')
            .textContent.trim());
    assertEquals(
        'Try installing the app again',
        component.shadowRoot.querySelector('#shimless3pDiagErrorDialogBody')
            .textContent.trim());

    await clickButton('#shimless3pDiagErrorDialogButton');
    assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
    assertFalse(isAllButtonsDisabled);
  });

  // Test error dialog can be controlled by Enter and Escape (cancel event).
  for (const [name, actionOnDialog] of [
           ['Enter', pressEnterOnDialog],
           ['Cancel', cancelDialog],
  ]) {
    test(`ErrorDialogClosedBy${name}`, async () => {
      service.setShow3pDiagnosticsAppResult(
          Show3pDiagnosticsAppResult.kAppNotInstalled);
      await initialize();
      assertTrue(!!component);

      component.launch3pDiagnostics();
      await flushTasks();
      assertTrue(isDialogOpen('#shimless3pDiagErrorDialog'));

      await actionOnDialog('#shimless3pDiagErrorDialog');
      await flushTasks();
      assertFalse(isDialogOpen('#shimless3pDiagErrorDialog'));
      assertFalse(isAllButtonsDisabled);
    });
  }

  // Test wrong shortcut don't trigger 3p diag.
  for (const {name, key, altKey, shiftKey} of
           [{
             name: 'NoAltKey',
             key: 'D',
             altKey: false,
             shiftKey: true,
           },
            {
              name: 'NoShiftKey',
              key: 'D',
              altKey: true,
              shiftKey: false,
            },
            // No 'D' key
            {
              name: 'NotDKey',
              key: 'X',
              altKey: true,
              shiftKey: true,
            },
            // No 'D' key, 'L' is for logging dialog.
            {
              name: 'LKey',
              key: 'L',
              altKey: true,
              shiftKey: true,
            },
  ]) {
    test(`WrongShortcutDoesntTrigger3pDiag${name}`, async () => {
      await initialize();
      assertTrue(!!component);

      await pressKey(key, altKey, shiftKey);
      assertFalse(hasDisabledAllButtons);
    });
  }

  // Verify 3p diag flow by trigger keyboard shortcut.
  for (const {name, key, altKey, shiftKey} of
           [{
             name: 'UppercaseD',
             key: 'D',
             altKey: true,
             shiftKey: true,
           },
            {
              name: 'LowercaseD',
              key: 'd',
              altKey: true,
              shiftKey: true,
            },
  ]) {
    test(`Launch3pDiagByShortcut${name}`, async () => {
      service.setShow3pDiagnosticsAppResult(Show3pDiagnosticsAppResult.kOk);
      await initialize();
      assertTrue(!!component);

      await pressKey(key, altKey, shiftKey);
      await flushTasks();
      assertTrue(hasDisabledAllButtons);
      assertTrue(service.wasShow3pDiagnosticsAppCalled());
      assertFalse(isAllButtonsDisabled);
    });
  }
});
