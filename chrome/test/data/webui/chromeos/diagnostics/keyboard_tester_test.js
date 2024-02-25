// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/keyboard_tester.js';
import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConnectionType, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey} from 'chrome://diagnostics/input.mojom-webui.js';
import {KeyEvent, KeyEventType} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {TopRightKey as DiagramTopRightKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {MockController} from '../mock_controller.m.js';
import {isVisible} from '../test_util.js';

suite('keyboardTesterTestSuite', function() {
  /** @type {?KeyboardTesterElement} */
  let keyboardTesterElement = null;

  const fakeKeyboard = {
    id: 4,
    connectionType: ConnectionType.kInternal,
    name: 'AT Translated Set 2 keyboard',
    physicalLayout: PhysicalLayout.kChromeOS,
    mechanicalLayout: MechanicalLayout.kAnsi,
    hasAssistantKey: false,
    numberPadPresent: NumberPadPresence.kNotPresent,
    topRowKeys: [],
    topRightKey: TopRightKey.kPower,
  };

  setup(() => {
    keyboardTesterElement = /** @type {?KeyboardTesterElement} */ (
        document.createElement('keyboard-tester'));
    document.body.appendChild(keyboardTesterElement);
  });

  /**
   * @param {boolean} isLoggedIn
   * @return {!Promise}
   */
  function setLoggedInState(isLoggedIn) {
    keyboardTesterElement.isLoggedIn = isLoggedIn;
    return flushTasks();
  }

  test('topRightKeyCorrections', async () => {
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      topRightKey: TopRightKey.kPower,
    });
    await flushTasks();

    const diagramElement =
        keyboardTesterElement.shadowRoot.querySelector('#diagram');
    assertEquals(DiagramTopRightKey.POWER, diagramElement.topRightKey);

    /** @type {!KeyEvent} */
    const lockKeyEvent = {
      id: 0,
      type: KeyEventType.kPress,
      keyCode: 142,
      scanCode: 0,
      topRowPosition: -1,
    };
    keyboardTesterElement.onKeyEvent(lockKeyEvent);
    await flushTasks();

    assertEquals(DiagramTopRightKey.LOCK, diagramElement.topRightKey);
  });

  test('f13Remapping', async () => {
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      topRightKey: TopRightKey.kLock,
    });
    await flushTasks();

    const diagramElement =
        keyboardTesterElement.shadowRoot.querySelector('#diagram');
    const mockController = new MockController();
    const mockSetKeyState =
        mockController.createFunctionMock(diagramElement, 'setKeyState');
    mockSetKeyState.addExpectation(
        142 /* KEY_SLEEP */, KeyboardKeyState.PRESSED);

    /** @type {!KeyEvent} */
    const f13Event = {
      id: 0,
      type: KeyEventType.kPress,
      keyCode: 183 /* KEY_F13 */,
      scanCode: 0,
      topRowPosition: 12,
    };
    keyboardTesterElement.onKeyEvent(f13Event);
    await flushTasks();

    mockController.verifyMocks();
    mockController.reset();
  });

  test('numberPadCorrection', async () => {
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      numberPadPresent: NumberPadPresence.kNotPresent,
    });
    await flushTasks();

    const diagramElement =
        keyboardTesterElement.shadowRoot.querySelector('#diagram');
    assertFalse(diagramElement.showNumberPad);

    /** @type {!KeyEvent} */
    const plusKeyEvent = {
      id: 0,
      type: KeyEventType.kPress,
      keyCode: 78 /* KEY_KPPLUS */,
      scanCode: 0,
      topRowPosition: -1,
    };
    keyboardTesterElement.onKeyEvent(plusKeyEvent);
    await flushTasks();

    assertTrue(diagramElement.showNumberPad);
  });

  test('numberPadCorrection_normalCrOS', async () => {
    // The delete key should make the number pad appear, unless it's a Dell
    // Enterprise keyboard.
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      physicalLayout: PhysicalLayout.kChromeOS,
      numberPadPresent: NumberPadPresence.kNotPresent,
    });
    await flushTasks();

    const diagramElement =
        keyboardTesterElement.shadowRoot.querySelector('#diagram');
    assertFalse(diagramElement.showNumberPad);

    /** @type {!KeyEvent} */
    const deleteKeyEvent = {
      id: 0,
      type: KeyEventType.kPress,
      keyCode: 111 /* KEY_DELETE */,
      scanCode: 0,
      topRowPosition: -1,
    };
    keyboardTesterElement.onKeyEvent(deleteKeyEvent);
    await flushTasks();

    assertTrue(diagramElement.showNumberPad);
  });

  test('numberPadCorrection_dellEnterprise', async () => {
    // The delete key should *not* make the number pad appear on a Dell
    // Enterprise keyboard.
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      physicalLayout: PhysicalLayout.kChromeOSDellEnterpriseWilco,
      numberPadPresent: NumberPadPresence.kNotPresent,
    });
    await flushTasks();

    const diagramElement =
        keyboardTesterElement.shadowRoot.querySelector('#diagram');
    assertFalse(diagramElement.showNumberPad);

    /** @type {!KeyEvent} */
    const deleteKeyEvent = {
      id: 0,
      type: KeyEventType.kPress,
      keyCode: 111 /* KEY_DELETE */,
      scanCode: 0,
      topRowPosition: -1,
    };
    keyboardTesterElement.onKeyEvent(deleteKeyEvent);
    await flushTasks();

    assertFalse(diagramElement.showNumberPad);
  });

  test('focusLossToast', async () => {
    keyboardTesterElement.keyboard = fakeKeyboard;
    await flushTasks();
    const mockTimer = new MockTimer();
    mockTimer.install();
    keyboardTesterElement.keyboard = fakeKeyboard;

    keyboardTesterElement.onKeyEventsPaused();
    assertTrue(keyboardTesterElement.$.lostFocusToast.open);

    keyboardTesterElement.onKeyEventsResumed();
    mockTimer.tick(1000);
    assertFalse(keyboardTesterElement.$.lostFocusToast.open);
    mockTimer.uninstall();
  });

  test('closeOnExitShortcut', async () => {
    keyboardTesterElement.keyboard = fakeKeyboard;
    await flushTasks();

    keyboardTesterElement.show();
    await flushTasks();
    assertTrue(keyboardTesterElement.isOpen());

    // Alt + Escape should close the tester
    const keyDownEvent = eventToPromise('keydown', keyboardTesterElement);

    keyboardTesterElement.dispatchEvent(new KeyboardEvent(
        'keydown', {bubbles: true, key: 'Escape', altKey: true}));
    await keyDownEvent;
    assertFalse(keyboardTesterElement.isOpen());
  });

  test('helpLinkIsHiddenWhenNotLoggedIn', async () => {
    keyboardTesterElement.keyboard = fakeKeyboard;
    await setLoggedInState(/** isLoggedIn */ false);

    keyboardTesterElement.show();
    await flushTasks();
    assertTrue(keyboardTesterElement.isOpen());
    const helpLink = keyboardTesterElement.shadowRoot.querySelector('#help');
    assertTrue(!!helpLink);
    assertFalse(isVisible(helpLink));

    keyboardTesterElement.close();
    await setLoggedInState(/** isLoggedIn */ true);
    keyboardTesterElement.show();
    await flushTasks();
    assertTrue(isVisible(helpLink));
  });
});
