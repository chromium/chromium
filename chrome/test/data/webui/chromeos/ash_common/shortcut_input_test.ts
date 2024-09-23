// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeShortcutInputProvider} from 'chrome://resources/ash/common/shortcut_input_ui/fake_shortcut_input_provider.js';
import {ShortcutInputElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import {ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {KeyInputState, Modifier} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

function getConfirmKeyElement(shortcutInputElement: ShortcutInputElement|
                              null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#confirmKey');
}

function getPendingKeyElement(shortcutInputElement: ShortcutInputElement|
                              null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#pendingKey');
}

function getCtrlElement(shortcutInputElement: ShortcutInputElement|
                        null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#ctrlKey');
}

function getShiftElement(shortcutInputElement: ShortcutInputElement|
                         null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#shiftKey');
}

function getAltElement(shortcutInputElement: ShortcutInputElement|
                       null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#altKey');
}

function getSearchElement(shortcutInputElement: ShortcutInputElement|
                          null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#searchKey');
}

function getFunctionElement(shortcutInputElement: ShortcutInputElement|
                            null): ShortcutInputKeyElement|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#functionKey');
}

function getKeySeparator(shortcutInputElement: ShortcutInputElement|
                         null): Element|null {
  if (shortcutInputElement === null) {
    return null;
  }
  return shortcutInputElement!.shadowRoot!.querySelector('#keySeparator');
}

suite('ShortcutInput', function() {
  let shortcutInputElement: ShortcutInputElement|null = null;
  const shortcutInputProvider: FakeShortcutInputProvider =
      new FakeShortcutInputProvider();
  let numShortcutInputEvents: number = 0;
  let numCaptureStateEvents: number = 0;
  let lastCaptureState: boolean = false;

  function initInputKeyElement(
      shortcutInputProvider: FakeShortcutInputProvider): ShortcutInputElement {
    const element = document.createElement('shortcut-input');
    document.body.appendChild(element);
    element.shortcutInputProvider = shortcutInputProvider;
    element.showSeparator = true;
    element.hasFunctionKey = true;
    element.addEventListener('shortcut-input-event', function() {
      ++numShortcutInputEvents;
    });
    element.addEventListener('shortcut-input-capture-state', function(e: any) {
      ++numCaptureStateEvents;
      lastCaptureState = e.detail.capturing;
    });
    flush();
    return element;
  }

  setup(async () => {
    // TODO(dpad, b/223455415): Provide a way to get the real loadTimeData in
    // ash_common browser tests.
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {
        'inputKeyPlaceholder': 'key',
        'iconLabelOpenLauncher': 'launcher',
        'iconLabelOpenSearch': 'search',
        'iconLabelPrintScreen': 'take screenshot',
      };
    }

    shortcutInputElement = initInputKeyElement(shortcutInputProvider);
    await flushTasks();
  });

  teardown(() => {
    if (shortcutInputElement) {
      shortcutInputElement.remove();
    }
    numShortcutInputEvents = 0;
    numCaptureStateEvents = 0;
    shortcutInputElement = null;
  });

  test('EditingKeysNotVisibleWhenNotCapturing', async () => {
    const pendingKey: ShortcutInputKeyElement|null =
        getPendingKeyElement(shortcutInputElement);
    const ctrlKey = getCtrlElement(shortcutInputElement);
    const altKey = getAltElement(shortcutInputElement);
    const searchKey = getSearchElement(shortcutInputElement);
    const shiftKey = getShiftElement(shortcutInputElement);
    const functionKey = getFunctionElement(shortcutInputElement);
    assertFalse(isVisible(pendingKey));
    assertFalse(isVisible(ctrlKey));
    assertFalse(isVisible(altKey));
    assertFalse(isVisible(searchKey));
    assertFalse(isVisible(shiftKey));
    assertFalse(isVisible(functionKey));
  });

  test('DisplayAlphaKey', async () => {
    shortcutInputElement!.startObserving();

    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    await flushTasks();

    const pendingKey: ShortcutInputKeyElement|null =
        getPendingKeyElement(shortcutInputElement);
    assertTrue(isVisible(pendingKey));
    assertEquals('a', pendingKey!.key);

    const ctrlKey = getCtrlElement(shortcutInputElement);
    const altKey = getAltElement(shortcutInputElement);
    const searchKey = getSearchElement(shortcutInputElement);
    const shiftKey = getShiftElement(shortcutInputElement);
    const functionKey = getFunctionElement(shortcutInputElement);
    // All keys should be visible and not selected.
    assertTrue(isVisible(ctrlKey));
    assertTrue(isVisible(altKey));
    assertTrue(isVisible(searchKey));
    assertTrue(isVisible(shiftKey));
    assertTrue(isVisible(functionKey));
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, searchKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, functionKey!.keyState);

    const confirmContainer =
        shortcutInputElement!.shadowRoot!.querySelector('#confirmContainer');
    assertFalse(isVisible(confirmContainer));
  });

  test('DisplayAlphaAndModifierKey', async () => {
    shortcutInputElement!.startObserving();

    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: Modifier.SHIFT,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    await flushTasks();

    const pendingKey: ShortcutInputKeyElement|null =
        getPendingKeyElement(shortcutInputElement);
    assertTrue(isVisible(pendingKey));
    assertEquals('a', pendingKey!.key);

    const ctrlKey = getCtrlElement(shortcutInputElement);
    const altKey = getAltElement(shortcutInputElement);
    const searchKey = getSearchElement(shortcutInputElement);
    const functionKey = getFunctionElement(shortcutInputElement);
    const shiftKey = getShiftElement(shortcutInputElement);

    // Only shift key should be selected, but all should be visible.
    assertTrue(isVisible(ctrlKey));
    assertTrue(isVisible(altKey));
    assertTrue(isVisible(searchKey));
    assertTrue(isVisible(functionKey));
    assertTrue(isVisible(shiftKey));
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, searchKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, functionKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, shiftKey!.keyState);

    const confirmContainer =
        shortcutInputElement!.shadowRoot!.querySelector('#confirmContainer');
    assertFalse(isVisible(confirmContainer));
  });

  test('DisplayAlphaAndAllModifierKeys', async () => {
    shortcutInputElement!.startObserving();

    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: Modifier.SHIFT | Modifier.CONTROL | Modifier.ALT |
          Modifier.COMMAND | Modifier.FN_KEY,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    await flushTasks();

    const pendingKey: ShortcutInputKeyElement|null =
        getPendingKeyElement(shortcutInputElement);
    assertTrue(isVisible(pendingKey));
    assertEquals('a', pendingKey!.key);

    const ctrlKey = getCtrlElement(shortcutInputElement);
    const altKey = getAltElement(shortcutInputElement);
    const searchKey = getSearchElement(shortcutInputElement);
    const shiftKey = getShiftElement(shortcutInputElement);
    const functionKey = getFunctionElement(shortcutInputElement);

    // All modifier keys should be selected.
    assertTrue(isVisible(ctrlKey));
    assertTrue(isVisible(altKey));
    assertTrue(isVisible(searchKey));
    assertTrue(isVisible(shiftKey));
    assertTrue(isVisible(functionKey));
    assertEquals(KeyInputState.MODIFIER_SELECTED, ctrlKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, altKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, searchKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, shiftKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, functionKey!.keyState);

    const confirmContainer =
        shortcutInputElement!.shadowRoot!.querySelector('#confirmContainer');
    assertFalse(isVisible(confirmContainer));
  });

  test('EventNotEmittedOnKeyPress', async () => {
    shortcutInputElement!.startObserving();
    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    await flushTasks();
    assertEquals(0, numShortcutInputEvents);
  });

  test('EventEmittedOnKeyRelease', async () => {
    shortcutInputElement!.startObserving();
    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
    await flushTasks();
    assertEquals(1, numShortcutInputEvents);
  });

  test('EventEmittedOnKeyPressWithUpdateOnPress', async () => {
    shortcutInputElement!.updateOnKeyPress = true;
    shortcutInputElement!.startObserving();
    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    await flushTasks();
    // Expect only one event to be sent.
    assertEquals(1, numShortcutInputEvents);

    // Still only expect one event to be sent.
    shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
    await flushTasks();
    assertEquals(1, numShortcutInputEvents);
  });

  test('EventNotEmittedOnModifierKeyRelease', async () => {
    shortcutInputElement!.startObserving();
    const keyEventPressed = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: Modifier.CONTROL,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEventPressed, keyEventPressed);

    const keyEventReleased = {
      vkey: VKey.kControl,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'ctrl',
    };
    shortcutInputProvider.sendKeyReleaseEvent(
        keyEventReleased, keyEventReleased);
    await flushTasks();
    assertEquals(0, numShortcutInputEvents);

    const keyEventNoModifierReleased = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyReleaseEvent(
        keyEventNoModifierReleased, keyEventNoModifierReleased);
    await flushTasks();
    assertEquals(1, numShortcutInputEvents);
  });

  test('EventEmittedOnCapturingStarted', async () => {
    shortcutInputElement!.startObserving();
    await flushTasks();

    assertEquals(1, numCaptureStateEvents);
    assertTrue(lastCaptureState);
  });

  test('EventEmittedOnCapturingStopped', async () => {
    shortcutInputElement!.startObserving();
    shortcutInputElement!.stopObserving();
    await flushTasks();

    assertEquals(2, numCaptureStateEvents);
    assertFalse(lastCaptureState);
  });

  test('EventEmittedOnFocusLost', async () => {
    shortcutInputElement!.startObserving();
    shortcutInputElement!.blur();
    await flushTasks();

    assertEquals(2, numCaptureStateEvents);
    assertFalse(lastCaptureState);
  });

  test('EventNotEmittedOnFocusLostWhileIgnoringBlur', async () => {
    shortcutInputElement!.ignoreBlur = true;
    shortcutInputElement!.startObserving();
    shortcutInputElement!.blur();
    await flushTasks();

    assertEquals(1, numCaptureStateEvents);
  });

  test('ConfirmViewShownWhenNotCapturing', async () => {
    shortcutInputElement!.startObserving();
    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: Modifier.SHIFT,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
    shortcutInputElement!.stopObserving();
    await flushTasks();

    const confirmKey = getConfirmKeyElement(shortcutInputElement);
    assertTrue(isVisible(confirmKey));
    assertEquals('a', confirmKey!.key);
    assertTrue(isVisible(getKeySeparator(shortcutInputElement)));

    // Modifiers consist of only shift.
    assertEquals(1, shortcutInputElement!.modifiers.length);
    assertEquals('shift', shortcutInputElement!.modifiers[0]);
  });

  test('ConfirmViewShownAllModifiers', async () => {
    shortcutInputElement!.startObserving();
    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers:
          Modifier.SHIFT | Modifier.CONTROL | Modifier.ALT | Modifier.COMMAND,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
    shortcutInputElement!.stopObserving();
    await flushTasks();

    const confirmKey = getConfirmKeyElement(shortcutInputElement);
    assertTrue(isVisible(confirmKey));
    assertEquals('a', confirmKey!.key);
    assertTrue(isVisible(getKeySeparator(shortcutInputElement)));

    assertEquals(4, shortcutInputElement!.modifiers.length);
    assertEquals('meta', shortcutInputElement!.modifiers[0]);
    assertEquals('ctrl', shortcutInputElement!.modifiers[1]);
    assertEquals('alt', shortcutInputElement!.modifiers[2]);
    assertEquals('shift', shortcutInputElement!.modifiers[3]);
  });

  test('ConfirmViewShownNoModifiers', async () => {
    shortcutInputElement!.startObserving();
    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
    shortcutInputElement!.stopObserving();
    await flushTasks();

    const confirmKey = getConfirmKeyElement(shortcutInputElement);
    assertTrue(isVisible(confirmKey));
    assertEquals('a', confirmKey!.key);
    assertFalse(isVisible(getKeySeparator(shortcutInputElement)));
    assertEquals(0, shortcutInputElement!.modifiers.length);
  });

  test('ObservedPrerewrittenKeyEvent', async () => {
    shortcutInputElement!.startObserving();
    const prerewrittenKeyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: Modifier.CONTROL,
      keyDisplay: 'a',
    };

    const keyEvent = {
      vkey: VKey.kKeyB,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'b',
    };

    shortcutInputProvider.sendKeyPressEvent(prerewrittenKeyEvent, keyEvent);
    await flushTasks();
    // Check that the prerewritten event was observed correctly.
    assertEquals(
        prerewrittenKeyEvent, shortcutInputProvider.getPrerewrittenKeyEvent());

    const prerewrittenReleasedKeyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    shortcutInputProvider.sendKeyReleaseEvent(
        prerewrittenReleasedKeyEvent, keyEvent);
    await flushTasks();
    assertEquals(
        prerewrittenReleasedKeyEvent,
        shortcutInputProvider.getPrerewrittenKeyEvent());
  });

  test('PressAndReleaseSingleKeyWhenUpdateOnKeyPress', async () => {
    shortcutInputElement!.updateOnKeyPress = true;
    shortcutInputElement!.startObserving();

    const keyEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };

    // Press 'a', expect pendingKey keyDisplay is 'a'.
    shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
    await flushTasks();
    let pendingKey: ShortcutInputKeyElement|null =
        getPendingKeyElement(shortcutInputElement);
    assertTrue(isVisible(pendingKey));
    assertEquals('a', pendingKey!.key);

    // Release 'a', expect pendingKey keyDisplay has reset to 'key'.
    shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
    await flushTasks();
    pendingKey = getPendingKeyElement(shortcutInputElement);
    assertTrue(isVisible(pendingKey));
    assertEquals('key', pendingKey!.key);
  });

  test('PressAndReleaseMultipleKeysWhenUpdateOnKeyPress', async () => {
    shortcutInputElement!.updateOnKeyPress = true;
    shortcutInputElement!.startObserving();

    const keyAEvent = {
      vkey: VKey.kKeyA,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'a',
    };
    const keyBEvent = {
      vkey: VKey.kKeyB,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'b',
    };

    // Press 'a', expect pendingKey keyDisplay is 'a'.
    shortcutInputProvider.sendKeyPressEvent(keyAEvent, keyAEvent);
    await flushTasks();
    let pendingKey: ShortcutInputKeyElement|null =
        getPendingKeyElement(shortcutInputElement);
    assertEquals('a', pendingKey!.key);

    // Press 'b', expect pendingKey keyDisplay updates to 'b'.
    shortcutInputProvider.sendKeyPressEvent(keyBEvent, keyBEvent);
    await flushTasks();
    pendingKey = getPendingKeyElement(shortcutInputElement);
    assertEquals('b', pendingKey!.key);

    // Release 'a', expect pendingKey keyDisplay is reset to 'key'.
    shortcutInputProvider.sendKeyReleaseEvent(keyAEvent, keyAEvent);
    await flushTasks();
    pendingKey = getPendingKeyElement(shortcutInputElement);
    assertEquals('key', pendingKey!.key);

    // Release 'b', expect pendingKey keyDisplay still 'key'.
    shortcutInputProvider.sendKeyReleaseEvent(keyBEvent, keyBEvent);
    await flushTasks();
    pendingKey = getPendingKeyElement(shortcutInputElement);
    assertEquals('key', pendingKey!.key);
  });

  test('PressAndReleaseModifiersWhenUpdateOnKeyPress', async () => {
    shortcutInputElement!.updateOnKeyPress = true;
    shortcutInputElement!.startObserving();

    // Press and hold 'ctrl' and 'shift'.
    const keyPressEvent = {
      vkey: VKey.kControl,
      domCode: 0,
      domKey: 0,
      modifiers: 6,
      keyDisplay: 'Control',
    };
    // Expect 'ctrl' and 'shift' are highlighted.
    shortcutInputProvider.sendKeyPressEvent(keyPressEvent, keyPressEvent);
    await flushTasks();
    let ctrlKey = getCtrlElement(shortcutInputElement);
    let shiftKey = getShiftElement(shortcutInputElement);
    assertEquals(KeyInputState.MODIFIER_SELECTED, ctrlKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, shiftKey!.keyState);

    // Release 'ctrl'
    const keyEventReleaseCtrl = {
      vkey: VKey.kControl,
      domCode: 0,
      domKey: 0,
      modifiers: 2,
      keyDisplay: 'Control',
    };
    // Expect 'ctrl' is unhighlighted, but 'shift' is still highlighted.
    shortcutInputProvider.sendKeyReleaseEvent(
        keyEventReleaseCtrl, keyEventReleaseCtrl);
    await flushTasks();
    ctrlKey = getCtrlElement(shortcutInputElement);
    shiftKey = getShiftElement(shortcutInputElement);
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey!.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, shiftKey!.keyState);

    // Release 'shift'
    const keyEventReleaseShift = {
      vkey: VKey.kShift,
      domCode: 0,
      domKey: 0,
      modifiers: 0,
      keyDisplay: 'Shift',
    };
    // Expect both 'ctrl' and 'shift' are unhighlighted.
    shortcutInputProvider.sendKeyReleaseEvent(
        keyEventReleaseShift, keyEventReleaseShift);
    await flushTasks();
    ctrlKey = getCtrlElement(shortcutInputElement);
    shiftKey = getShiftElement(shortcutInputElement);
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey!.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey!.keyState);
  });
});
