// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConnectionType, KeyEvent, KeyEventType, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeKeyboards} from 'chrome://diagnostics/fake_data.js';

import {TopRightKey as DiagramTopRightKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {MockController} from '../../mock_controller.js';
import {flushTasks} from '../../test_util.js';

export function keyboardTesterTestSuite() {
  /** @type {?KeyboardTesterElement} */
  let keyboardTesterElement = null;

  setup(() => {
    keyboardTesterElement = /** @type {?KeyboardTesterElement} */ (
        document.createElement('keyboard-tester'));
    document.body.appendChild(keyboardTesterElement);
  });

  test('topRightKeyCorrections', async () => {
    keyboardTesterElement.keyboard = {
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
    keyboardTesterElement.keyboard = {
      id: 4,
      connectionType: ConnectionType.kInternal,
      name: 'AT Translated Set 2 keyboard',
      physicalLayout: PhysicalLayout.kChromeOS,
      mechanicalLayout: MechanicalLayout.kAnsi,
      hasAssistantKey: false,
      numberPadPresent: NumberPadPresence.kNotPresent,
      topRowKeys: [],
      topRightKey: TopRightKey.kLock,
    };
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
}
