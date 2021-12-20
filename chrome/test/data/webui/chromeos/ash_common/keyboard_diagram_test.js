// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MechanicalLayout, PhysicalLayout, TopRowKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {assertEquals, assertNotEquals, assertTrue} from '../../chai_assert.js';
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

  test('dell-enterprise', async () => {
    assertKeyHidden('dellPageDownKey');
    assertKeyHidden('dellPageUpKey');
    assertKeyHidden('fnKey');
    assertKeyHidden('layoutSwitchKey');

    diagramElement.physicalLayout = PhysicalLayout.kChromeOSDellEnterprise;
    await flushTasks();

    assertKeyVisible('dellPageDownKey');
    assertKeyVisible('dellPageUpKey');
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
}
