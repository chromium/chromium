// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MechanicalLayout, PhysicalLayout, TopRowKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {assertEquals, assertNotEquals, assertThrows, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

export function keyboardDiagramTestSuite() {
  /** @type {?KeyboardDiagramElement} */
  let diagramElement = null;

  setup(() => {
    diagramElement = /** @type {!KeyboardDiagramElement} */ (
        document.createElement('keyboard-diagram'));
    document.body.appendChild(diagramElement);
  });

  teardown(() => {
    diagramElement.remove();
    diagramElement = null;
  });

  function assertKeyHidden(keyId) {
    const key = diagramElement.root.getElementById(keyId);
    assertTrue(
        key == null || key.offsetHeight === 0 || key.offsetWidth === 0,
        `Expected key with ID '${keyId}' to be hidden, but it is visible`);
  }

  function assertKeyVisible(keyId) {
    const key = diagramElement.root.getElementById(keyId);
    assertTrue(
        !!key && key.offsetHeight > 0 && key.offsetWidth > 0,
        `Expected key with ID '${keyId}' to be visible, but it is hidden`);
  }

  function assertJisKeysHidden() {
    assertKeyHidden('jisAlphanumericKey');
    assertKeyHidden('jisBackslashKey');
    assertKeyHidden('jisKanaKey');
    assertKeyHidden('jisYenKey');
  }

  test('ansi', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.kAnsi;
    await flushTasks();
    assertKeyHidden('enterKeyLowerPart');
    assertKeyHidden('isoKey');
    assertJisKeysHidden();
  });

  test('iso', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.kIso;
    await flushTasks();
    assertKeyVisible('enterKeyLowerPart');
    assertKeyVisible('isoKey');
    assertJisKeysHidden();
  });

  test('jis', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.kJis;
    await flushTasks();
    assertKeyVisible('enterKeyLowerPart');
    assertKeyHidden('isoKey');
    assertKeyVisible('jisAlphanumericKey');
    assertKeyVisible('jisBackslashKey');
    assertKeyVisible('jisKanaKey');
    assertKeyVisible('jisYenKey');
  });

  test('dellEnterpriseWilco', async () => {
    assertKeyHidden('dellPageDownKey');
    assertKeyHidden('dellPageUpKey');
    assertKeyHidden('fnKey');
    assertKeyHidden('layoutSwitchKey');

    diagramElement.physicalLayout = PhysicalLayout.kChromeOSDellEnterpriseWilco;
    await flushTasks();

    assertKeyVisible('dellPageDownKey');
    assertKeyVisible('dellPageUpKey');
    assertKeyVisible('fnKey');
    assertKeyVisible('layoutSwitchKey');
  });

  test('dellEnterpriseDrallion', async () => {
    assertKeyHidden('fnKey');
    assertKeyHidden('layoutSwitchKey');

    diagramElement.physicalLayout =
        PhysicalLayout.kChromeOSDellEnterpriseDrallion;
    await flushTasks();

    assertKeyHidden('dellPageDownKey');
    assertKeyHidden('dellPageUpKey');
    assertKeyVisible('fnKey');
    assertKeyVisible('layoutSwitchKey');
  });

  test('resize', async () => {
    const keyboardElement = diagramElement.root.getElementById('keyboard');
    diagramElement.showNumberPad = false;

    document.body.style.width = '700px';
    await waitAfterNextRender(keyboardElement);
    assertEquals(264, keyboardElement.offsetHeight);

    document.body.style.width = '1000px';
    await waitAfterNextRender(keyboardElement);
    assertEquals(377, keyboardElement.offsetHeight);
  });

  test('resizeOnNumpadChange', async () => {
    const keyboardElement = diagramElement.root.getElementById('keyboard');

    document.body.style.width = '1000px';
    diagramElement.showNumberPad = false;
    await waitAfterNextRender(keyboardElement);
    assertEquals(377, keyboardElement.offsetHeight);

    diagramElement.showNumberPad = true;
    await waitAfterNextRender(keyboardElement);
    assertEquals(290, keyboardElement.offsetHeight);
  });

  test('topRowKeys', async () => {
    const topRowContainer = diagramElement.$.topRow;
    const testKeySet = [
      TopRowKey.kBack,
      TopRowKey.kRefresh,
      TopRowKey.kNone,
      TopRowKey.kNone,
      TopRowKey.kScreenMirror,
      TopRowKey.kDelete,
    ];

    diagramElement.topRowKeys = testKeySet;
    await flushTasks();

    const keyElements = topRowContainer.getElementsByTagName('keyboard-key');
    // Add 2 for the escape and power keys, which are in the same container.
    assertEquals(testKeySet.length + 2, keyElements.length);

    assertEquals('keyboard:back', keyElements[1].icon);
    assertEquals('delete', keyElements[6].mainGlyph);
  });

  test('setKeyState', async () => {
    const backspaceKey = diagramElement.root.getElementById('backspaceKey');
    assertEquals(KeyboardKeyState.kNotPressed, backspaceKey.state);
    diagramElement.setKeyState(
        14 /* KEY_BACKSPACE */, KeyboardKeyState.kPressed);
    assertEquals(KeyboardKeyState.kPressed, backspaceKey.state);
  });

  test('setKeyState_twoPartEnter', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.kIso;
    await flushTasks();

    const enterKey = diagramElement.root.getElementById('enterKey');
    const enterKeyLowerPart =
        diagramElement.root.getElementById('enterKeyLowerPart');
    assertEquals(KeyboardKeyState.kNotPressed, enterKey.state);
    assertEquals(KeyboardKeyState.kNotPressed, enterKeyLowerPart.state);
    diagramElement.setKeyState(28 /* KEY_ENTER */, KeyboardKeyState.kPressed);
    assertEquals(KeyboardKeyState.kPressed, enterKey.state);
    assertEquals(KeyboardKeyState.kPressed, enterKeyLowerPart.state);
  });

  test('setTopRowKeyState', async () => {
    const topRowContainer = diagramElement.$.topRow;
    const testKeySet = [
      TopRowKey.kBack,
      TopRowKey.kRefresh,
      TopRowKey.kNone,
      TopRowKey.kNone,
      TopRowKey.kScreenMirror,
      TopRowKey.kDelete,
    ];

    diagramElement.topRowKeys = testKeySet;
    await flushTasks();

    diagramElement.setTopRowKeyState(
        /* topRowPosition= */ 0, KeyboardKeyState.kPressed);
    const keyElements = topRowContainer.getElementsByTagName('keyboard-key');
    assertEquals(KeyboardKeyState.kPressed, keyElements[1].state);
  });

  test('setTopRowKeyState_invalidPosition', async () => {
    const topRowContainer = diagramElement.$.topRow;
    const testKeySet = [
      TopRowKey.kBack,
      TopRowKey.kRefresh,
      TopRowKey.kNone,
      TopRowKey.kNone,
      TopRowKey.kScreenMirror,
      TopRowKey.kDelete,
    ];

    diagramElement.topRowKeys = testKeySet;
    await flushTasks();

    assertThrows(
        () => diagramElement.setTopRowKeyState(
            /* topRowPosition= */ -1, KeyboardKeyState.kPressed),
        RangeError);
    assertThrows(
        () => diagramElement.setTopRowKeyState(
            /* topRowPosition= */ testKeySet.length + 1,
            KeyboardKeyState.kPressed),
        RangeError);
  });

  test('clearPressedKeys', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.kIso;
    diagramElement.topRowKeys = [
      TopRowKey.kBack,
      TopRowKey.kRefresh,
      TopRowKey.kOverview,
    ];
    await flushTasks();

    diagramElement.setKeyState(28 /* KEY_ENTER */, KeyboardKeyState.kPressed);
    diagramElement.setKeyState(56 /* KEY_LEFTALT */, KeyboardKeyState.kPressed);
    diagramElement.setKeyState(15 /* KEY_TAB */, KeyboardKeyState.kPressed);
    diagramElement.setTopRowKeyState(2, KeyboardKeyState.kPressed);
    diagramElement.clearPressedKeys();
    await flushTasks();

    const pressedKeys = diagramElement.root.querySelectorAll(
        `keyboard-key[state="${KeyboardKeyState.kPressed}"]`);
    assertEquals(0, pressedKeys.length);
  });
}
