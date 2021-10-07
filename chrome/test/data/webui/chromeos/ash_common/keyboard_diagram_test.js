// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MechanicalLayout} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {assertEquals, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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
}
