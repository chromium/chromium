// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConnectionType, KeyEvent, KeyEventType, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey} from 'chrome://diagnostics/diagnostics_types.js';
import {TopRightKey as DiagramTopRightKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {MockController} from '../../mock_controller.js';
import {flushTasks} from '../../test_util.js';

export function keyboardTesterTestSuite() {
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

  test('topRightKeyCorrections', async () => {
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      topRightKey: TopRightKey.kPower,
    });
    await flushTasks();

    const diagramElement = keyboardTesterElement.$$('#diagram');
    assertEquals(DiagramTopRightKey.kPower, diagramElement.topRightKey);

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

    assertEquals(DiagramTopRightKey.kLock, diagramElement.topRightKey);
  });

  test('f13Remapping', async () => {
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      topRightKey: TopRightKey.kLock,
    });
    await flushTasks();

    const diagramElement = keyboardTesterElement.$$('#diagram');
    const mockController = new MockController();
    const mockSetKeyState =
        mockController.createFunctionMock(diagramElement, 'setKeyState');
    mockSetKeyState.addExpectation(
        142 /* KEY_SLEEP */, KeyboardKeyState.kPressed);

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

    const diagramElement = keyboardTesterElement.$$('#diagram');
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

    const diagramElement = keyboardTesterElement.$$('#diagram');
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

    const diagramElement = keyboardTesterElement.$$('#diagram');
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

    keyboardTesterElement.onKeyEventsPaused();
    assertTrue(keyboardTesterElement.$.lostFocusToast.open);

    keyboardTesterElement.onKeyEventsResumed();
    assertFalse(keyboardTesterElement.$.lostFocusToast.open);
  });
}
