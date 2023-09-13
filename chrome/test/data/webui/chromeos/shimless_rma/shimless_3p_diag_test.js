// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {Shimless3pDiagnostics} from 'chrome://shimless-rma/shimless_3p_diagnostics.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
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

  const disableAllButtonsListener = () => {
    hasDisabledAllButtons = true;
  };

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
    window.addEventListener('disable-all-buttons', disableAllButtonsListener);
    hasDisabledAllButtons = false;

    loadTimeData.overrideValues({'3pDiagnosticsEnabled': true});
  });

  teardown(() => {
    window.removeEventListener(
        'disable-all-buttons', disableAllButtonsListener);
    component.remove();
    component = null;
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
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

  // Test wrong shortcut don't trigger 3p diag.
  const wrong_keyboard_shortcuts = [
    {
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
  ];
  for (const {name, key, altKey, shiftKey} of wrong_keyboard_shortcuts) {
    test(`WrongShortcutDontTrigger3pDiag${name}`, async () => {
      await initialize();
      assertTrue(!!component);

      const keydownEventPromise = eventToPromise('keydown', component);
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
      await keydownEventPromise;
      assertFalse(hasDisabledAllButtons);
    });
  }

  // Verify 3p diag flow by trigger keyboard shortcut.
  const correct_keyboard_shortcuts = [
    {
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
  ];
  for (const {name, key, altKey, shiftKey} of correct_keyboard_shortcuts) {
    test(`Launch3pDiagByShortcut${name}`, async () => {
      await initialize();
      assertTrue(!!component);

      const keydownEventPromise = eventToPromise('keydown', component);
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
      await keydownEventPromise;
      assertTrue(hasDisabledAllButtons);
      // TODO(chungsheng): Verify other things after implemented them.
    });
  }
});
