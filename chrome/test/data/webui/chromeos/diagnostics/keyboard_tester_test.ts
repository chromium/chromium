// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/keyboard_tester.js';
import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConnectionType, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey} from 'chrome://diagnostics/input.mojom-webui.js';
import {KeyEventType} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {KeyboardTesterElement} from 'chrome://diagnostics/keyboard_tester.js';
import {KeyboardDiagramElement, TopRightKey as DiagramTopRightKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('keyboardTesterTestSuite', function() {
  let keyboardTesterElement: KeyboardTesterElement|null = null;

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
    regionCode: 'jp',
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    keyboardTesterElement = document.createElement('keyboard-tester');
    document.body.appendChild(keyboardTesterElement);
  });

  function setLoggedInState(isLoggedIn: boolean): Promise<void> {
    assert(keyboardTesterElement);
    keyboardTesterElement.isLoggedIn = isLoggedIn;
    return flushTasks();
  }

  function getKeyboardDiagram(): KeyboardDiagramElement {
    assert(keyboardTesterElement);
    return strictQuery(
        '#diagram', keyboardTesterElement.shadowRoot, KeyboardDiagramElement);
  }

  test('topRightKeyCorrections', async () => {
    assert(keyboardTesterElement);
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      topRightKey: TopRightKey.kPower,
    });
    await flushTasks();

    const diagramElement = getKeyboardDiagram();
    assert(diagramElement);
    assertEquals(DiagramTopRightKey.POWER, diagramElement.topRightKey);

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
    assert(keyboardTesterElement);
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      topRightKey: TopRightKey.kLock,
    });
    await flushTasks();

    const diagramElement = getKeyboardDiagram();
    assert(diagramElement);
    const mockController = new MockController();
    const mockSetKeyState =
        mockController.createFunctionMock(diagramElement, 'setKeyState');
    mockSetKeyState.addExpectation(
        142 /* KEY_SLEEP */, KeyboardKeyState.PRESSED);

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
    assert(keyboardTesterElement);
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      numberPadPresent: NumberPadPresence.kNotPresent,
    });
    await flushTasks();

    const diagramElement = getKeyboardDiagram();
    assert(diagramElement);
    assertFalse(diagramElement.showNumberPad);

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
    assert(keyboardTesterElement);
    // The delete key should make the number pad appear, unless it's a Dell
    // Enterprise keyboard.
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      physicalLayout: PhysicalLayout.kChromeOS,
      numberPadPresent: NumberPadPresence.kNotPresent,
    });
    await flushTasks();

    const diagramElement = getKeyboardDiagram();
    assert(diagramElement);
    assertFalse(diagramElement.showNumberPad);

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
    assert(keyboardTesterElement);
    // The delete key should *not* make the number pad appear on a Dell
    // Enterprise keyboard.
    keyboardTesterElement.keyboard = Object.assign({}, fakeKeyboard, {
      physicalLayout: PhysicalLayout.kChromeOSDellEnterpriseWilco,
      numberPadPresent: NumberPadPresence.kNotPresent,
    });
    await flushTasks();

    const diagramElement = getKeyboardDiagram();
    assert(diagramElement);
    assertFalse(diagramElement.showNumberPad);

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
    assert(keyboardTesterElement);
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
    assert(keyboardTesterElement);
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
    assert(keyboardTesterElement);
    keyboardTesterElement.keyboard = fakeKeyboard;
    await setLoggedInState(/** isLoggedIn */ false);

    keyboardTesterElement.show();
    await flushTasks();
    assertTrue(keyboardTesterElement.isOpen());
    const helpLink = keyboardTesterElement.shadowRoot!.querySelector('#help');
    assertTrue(!!helpLink);
    assertFalse(isVisible(helpLink));

    keyboardTesterElement.close();
    await setLoggedInState(/** isLoggedIn */ true);
    keyboardTesterElement.show();
    await flushTasks();
    assertTrue(isVisible(helpLink));
  });
});
