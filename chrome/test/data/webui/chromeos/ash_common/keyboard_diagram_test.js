// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MechanicalLayout, PhysicalLayout, TopRightKey, TopRowKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertNotEquals, assertThrows, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('keyboardDiagramTestSuite', () => {
  /** @type {?KeyboardDiagramElement} */
  let diagramElement = null;

  setup(() => {
    // TODO(b/223455415): serve the Ash common JavaScript from its own WebUI
    // request handler for the tests, to avoid having to duplicate the strings
    // here.
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {
        'keyboardDiagramAriaLabelNotPressed': '$1 key not pressed',
        'keyboardDiagramAriaLabelPressed': '$1 key pressed',
        'keyboardDiagramAriaLabelTested': '$1 key tested',
        'keyboardDiagramAriaNameArrowDown': 'Down arrow',
        'keyboardDiagramAriaNameArrowLeft': 'Left arrow',
        'keyboardDiagramAriaNameArrowRight': 'Right arrow',
        'keyboardDiagramAriaNameArrowUp': 'Up arrow',
        'keyboardDiagramAriaNameAssistant': 'Assistant',
        'keyboardDiagramAriaNameBack': 'Back',
        'keyboardDiagramAriaNameBackspace': 'Backspace',
        'keyboardDiagramAriaNameControlPanel': 'Control Panel',
        'keyboardDiagramAriaNameEnter': 'Enter',
        'keyboardDiagramAriaNameForward': 'Forward',
        'keyboardDiagramAriaNameFullscreen': 'Fullscreen',
        'keyboardDiagramAriaNameJisLetterSwitch': 'Kana/alphanumeric switch',
        'keyboardDiagramAriaNameKeyboardBacklightDown':
            'Keyboard brightness down',
        'keyboardDiagramAriaNameKeyboardBacklightUp': 'Keyboard brightness up',
        'keyboardDiagramAriaNameLauncher': 'Launcher',
        'keyboardDiagramAriaNameLayoutSwitch': 'Layout switch',
        'keyboardDiagramAriaNameLock': 'Lock',
        'keyboardDiagramAriaNameMute': 'Mute',
        'keyboardDiagramAriaNameOverview': 'Overview',
        'keyboardDiagramAriaNamePlayPause': 'Play/Pause',
        'keyboardDiagramAriaNamePower': 'Power',
        'keyboardDiagramAriaNamePrivacyScreenToggle': 'Privacy screen toggle',
        'keyboardDiagramAriaNameRefresh': 'Refresh',
        'keyboardDiagramAriaNameScreenBrightnessDown':
            'Display brightness down',
        'keyboardDiagramAriaNameScreenBrightnessUp': 'Display brightness up',
        'keyboardDiagramAriaNameScreenMirror': 'Screen mirror',
        'keyboardDiagramAriaNameScreenshot': 'Screenshot',
        'keyboardDiagramAriaNameShiftLeft': 'Left shift',
        'keyboardDiagramAriaNameShiftRight': 'Right shift',
        'keyboardDiagramAriaNameTab': 'Tab',
        'keyboardDiagramAriaNameTrackNext': 'Next track',
        'keyboardDiagramAriaNameTrackPrevious': 'Previous track',
        'keyboardDiagramAriaNameVolumeDown': 'Volume down',
        'keyboardDiagramAriaNameVolumeUp': 'Volume up',
      };
    }

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
    diagramElement.mechanicalLayout = MechanicalLayout.ANSI;
    await flushTasks();
    assertKeyHidden('enterKeyLowerPart');
    assertKeyHidden('isoKey');
    assertJisKeysHidden();
  });

  test('iso', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.ISO;
    await flushTasks();
    assertKeyVisible('enterKeyLowerPart');
    assertKeyVisible('isoKey');
    assertJisKeysHidden();
  });

  test('jis', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.JIS;
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

    diagramElement.physicalLayout =
        PhysicalLayout.CHROME_OS_DELL_ENTERPRISE_WILCO;
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
        PhysicalLayout.CHROME_OS_DELL_ENTERPRISE_DRALLION;
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
    assertEquals('Back', keyElements[1].ariaName);
    assertEquals('delete', keyElements[6].mainGlyph);
  });

  test('topRightKeyAppearsDisabled', async () => {
    diagramElement.topRightKey = TopRightKey.POWER;
    await flushTasks();

    const topRightKey = diagramElement.$.topRightKey;
    assertEquals(undefined, topRightKey.icon);
    assertEquals(undefined, topRightKey.ariaName);

    diagramElement.setKeyState(116 /* KEY_POWER */, KeyboardKeyState.PRESSED);
    assertEquals(KeyboardKeyState.NOT_PRESSED, topRightKey.state);
  });

  test('setKeyState', async () => {
    const backspaceKey = diagramElement.root.getElementById('backspaceKey');
    assertEquals(KeyboardKeyState.NOT_PRESSED, backspaceKey.state);
    diagramElement.setKeyState(
        14 /* KEY_BACKSPACE */, KeyboardKeyState.PRESSED);
    assertEquals(KeyboardKeyState.PRESSED, backspaceKey.state);
  });

  test('setKeyState_twoPartEnter', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.ISO;
    await flushTasks();

    const enterKey = diagramElement.root.getElementById('enterKey');
    const enterKeyLowerPart =
        diagramElement.root.getElementById('enterKeyLowerPart');
    assertEquals(KeyboardKeyState.NOT_PRESSED, enterKey.state);
    assertEquals(KeyboardKeyState.NOT_PRESSED, enterKeyLowerPart.state);
    diagramElement.setKeyState(28 /* KEY_ENTER */, KeyboardKeyState.PRESSED);
    assertEquals(KeyboardKeyState.PRESSED, enterKey.state);
    assertEquals(KeyboardKeyState.PRESSED, enterKeyLowerPart.state);
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
        /* topRowPosition= */ 0, KeyboardKeyState.PRESSED);
    const keyElements = topRowContainer.getElementsByTagName('keyboard-key');
    assertEquals(KeyboardKeyState.PRESSED, keyElements[1].state);
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
            /* topRowPosition= */ -1, KeyboardKeyState.PRESSED),
        RangeError);
    assertThrows(
        () => diagramElement.setTopRowKeyState(
            /* topRowPosition= */ testKeySet.length + 1,
            KeyboardKeyState.PRESSED),
        RangeError);
  });

  test('clearPressedKeys', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.ISO;
    diagramElement.topRowKeys = [
      TopRowKey.kBack,
      TopRowKey.kRefresh,
      TopRowKey.kOverview,
    ];
    await flushTasks();

    diagramElement.setKeyState(28 /* KEY_ENTER */, KeyboardKeyState.PRESSED);
    diagramElement.setKeyState(56 /* KEY_LEFTALT */, KeyboardKeyState.PRESSED);
    diagramElement.setKeyState(15 /* KEY_TAB */, KeyboardKeyState.PRESSED);
    diagramElement.setTopRowKeyState(2, KeyboardKeyState.PRESSED);
    diagramElement.clearPressedKeys();
    await flushTasks();

    const pressedKeys = diagramElement.root.querySelectorAll(
        `keyboard-key[state="${KeyboardKeyState.PRESSED}"]`);
    assertEquals(0, pressedKeys.length);
  });

  test('resetAllKeys', async () => {
    diagramElement.mechanicalLayout = MechanicalLayout.ISO;
    diagramElement.topRowKeys = [
      TopRowKey.kBack,
      TopRowKey.kRefresh,
      TopRowKey.kOverview,
    ];
    await flushTasks();

    diagramElement.setKeyState(28 /* KEY_ENTER */, KeyboardKeyState.PRESSED);
    diagramElement.setKeyState(56 /* KEY_LEFTALT */, KeyboardKeyState.PRESSED);
    diagramElement.setKeyState(15 /* KEY_TAB */, KeyboardKeyState.TESTED);
    diagramElement.setTopRowKeyState(2, KeyboardKeyState.TESTED);
    diagramElement.resetAllKeys();
    await flushTasks();

    const pressedKeys = diagramElement.root.querySelectorAll(
        `keyboard-key[state="${KeyboardKeyState.PRESSED}"]`);
    assertEquals(0, pressedKeys.length);

    const testedKeys = diagramElement.root.querySelectorAll(
        `keyboard-key[state="${KeyboardKeyState.TESTED}"]`);
    assertEquals(0, testedKeys.length);
  });

  test('visualLayout_mainGlyph', async () => {
    diagramElement.regionCode = 'fr';

    const escKey = diagramElement.root.querySelector('[data-code="1"]');
    assertEquals('échap', escKey.mainGlyph);
  });

  test('visualLayout_enterKeyLowerPartDoesntGetGlyph', async () => {
    diagramElement.regionCode = 'us';

    const enterKeyLowerPart = diagramElement.$.enterKeyLowerPart;
    assertEquals(undefined, enterKeyLowerPart.mainGlyph);
  });

  test('visualLayout_iconAndCornerGlyphs', async () => {
    diagramElement.regionCode = 'jp';

    const letterSwitchKey =
        diagramElement.root.querySelector('[data-code="41"]');
    assertEquals('keyboard:jis-letter-switch', letterSwitchKey.icon);

    const slashKey = diagramElement.root.querySelector('[data-code="53"]');
    assertEquals('/', slashKey.bottomLeftGlyph);
    assertEquals('?', slashKey.topLeftGlyph);
    assertEquals('•', slashKey.topRightGlyph);
    assertEquals('め', slashKey.bottomRightGlyph);
  });

  test('visualLayout_ariaNames', async () => {
    diagramElement.regionCode = 'jp';

    const letterSwitchKey =
        diagramElement.root.querySelector('[data-code="41"]');
    assertEquals('Kana/alphanumeric switch', letterSwitchKey.ariaName);
  });
});
