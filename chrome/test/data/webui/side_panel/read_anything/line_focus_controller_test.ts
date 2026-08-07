// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusController, LineFocusModel, LineFocusMovement, LineFocusStyle, LineFocusType, ReadAloudNode, setInstance, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineFocusListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('LineFocusController', () => {
  const defaultHeight = 1000;
  let lineFocusController: LineFocusController;
  let lineFocusListener: LineFocusListener;
  let model: LineFocusModel;
  let lineFocusContentPositionChanged: boolean;
  let lineFocusVisualPositionChanged: boolean;
  let defaultContainer: HTMLElement;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let readAloudModel: TestReadAloudModelBrowserProxy;
  let metrics: TestMetricsBrowserProxy;
  let keyboardLines: number;
  let speechLines: number;
  let lineFocusModesChanged: boolean;

  function createShortContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'I\'ve heard it said\nThat people come into our lives\nfor a reason.';
    container.style.whiteSpace = 'pre-line';
    document.body.appendChild(container);
    return container;
  }

  function createLongContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'Bringing something we must learn\nAnd we are lead to those\n' +
        'who help us most to grow\nif we let them and we help them in return\n' +
        'Now I don\'t know if I believe that\'s true\n' +
        'But I know I\'m who I am today because I met you';
    container.style.whiteSpace = 'pre-line';
    document.body.appendChild(container);
    return container;
  }

  function keyDown(key: string): KeyboardEvent {
    return new KeyboardEvent('keydown', {key});
  }

  function toggleKey(): KeyboardEvent {
    return new KeyboardEvent('keydown', {key: 'l', altKey: true});
  }

  function downKey(): KeyboardEvent {
    return keyDown('ArrowDown');
  }

  function upKey(): KeyboardEvent {
    return keyDown('ArrowUp');
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isLineFocusEnabled = true;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    metrics = mockMetrics();
    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    readAloudModel.setInitialized(true);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    model = new LineFocusModel();
    lineFocusController = new LineFocusController(model);
    lineFocusContentPositionChanged = false;
    lineFocusVisualPositionChanged = false;
    lineFocusModesChanged = false;
    lineFocusListener = {
      onLineFocusContentPositionChange() {
        lineFocusContentPositionChanged = true;
      },
      onLineFocusVisualPositionChange() {
        lineFocusVisualPositionChanged = true;
      },
      onNeedScrollForLineFocus() {},
      onNeedScrollToTop() {},
      onLineFocusModesChanged() {
        lineFocusModesChanged = true;
      },
      onScrollBufferForLineFocusChange() {},
    };
    lineFocusController.addListener(lineFocusListener);
    defaultContainer = document.createElement('div');
    keyboardLines = 0;
    speechLines = 0;
    chrome.readingMode.incrementLineFocusKeyboardLines = () => keyboardLines++;
    chrome.readingMode.incrementLineFocusSpeechLines = () => speechLines++;
  });

  test('isEnabled is false by default', () => {
    assertFalse(lineFocusController.isEnabled());
  });

  test('isEnabled is true for line', () => {
    lineFocusController.toggle(true, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());
  });

  test('isEnabled is true for window', () => {
    lineFocusController.toggle(true, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());
  });

  test('isEnabled is false for off', () => {
    lineFocusController.toggle(true, defaultContainer, defaultHeight);

    lineFocusController.toggle(false, defaultContainer, defaultHeight);

    assertFalse(lineFocusController.isEnabled());
  });

  test('isEnabled is false with flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    lineFocusController.toggle(true, defaultContainer, defaultHeight);
    assertFalse(lineFocusController.isEnabled());
  });

  test('onStyleChange updates style only', () => {
    lineFocusController.toggle(false, defaultContainer, defaultHeight);
    const isStatic = lineFocusController.getCurrentLineFocusMovement() ===
        LineFocusMovement.STATIC;
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.UNDERLINE,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(
        isStatic,
        lineFocusController.getCurrentLineFocusMovement() ===
            LineFocusMovement.STATIC);
    assertFalse(lineFocusController.isEnabled());

    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.SMALL_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(
        isStatic,
        lineFocusController.getCurrentLineFocusMovement() ===
            LineFocusMovement.STATIC);
    assertFalse(lineFocusController.isEnabled());
  });

  test('onStyleChange propagates line focus mode', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);

    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusCursorLine,
        chrome.readingMode.lastNonDisabledLineFocus);

    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusLargeCursorWindow,
        chrome.readingMode.lastNonDisabledLineFocus);
  });

  test('style and movement changes do nothing with flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;

    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, defaultContainer, defaultHeight);
    assertEquals(0, chrome.readingMode.lastNonDisabledLineFocus);

    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    assertEquals(0, chrome.readingMode.lastNonDisabledLineFocus);
  });

  test('onMovementChange propagates line focus mode', () => {
    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, defaultContainer, defaultHeight);

    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusSmallCursorWindow,
        chrome.readingMode.lastNonDisabledLineFocus);

    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusSmallStaticWindow,
        chrome.readingMode.lastNonDisabledLineFocus);
  });

  test('onMovementChange updates movement only', () => {
    lineFocusController.toggle(false, defaultContainer, defaultHeight);
    const startingStyle = lineFocusController.getCurrentLineFocusStyle();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusMovement.CURSOR,
        lineFocusController.getCurrentLineFocusMovement());
    assertEquals(startingStyle, lineFocusController.getCurrentLineFocusStyle());

    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusMovement.STATIC,
        lineFocusController.getCurrentLineFocusMovement());
    assertEquals(startingStyle, lineFocusController.getCurrentLineFocusStyle());
  });

  test('onMovementChange to cursor updates position', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.toggle(true, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    assertEquals(0, model.getTop());
  });

  test('onMovementChange to static sets it in the middle', () => {
    lineFocusController.toggle(true, defaultContainer, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, defaultContainer, defaultHeight);

    assertEquals(defaultHeight / 2, model.getTop());
  });

  test('onStyleChange window sizes should be different heights', () => {
    const container = createShortContainer();
    lineFocusController.toggle(true, container, defaultHeight);

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    const height1 = model.getWindowHeight();
    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, container, defaultHeight);
    const height2 = model.getWindowHeight();
    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, container, defaultHeight);
    const height3 = model.getWindowHeight();

    assertTrue(!!height1);
    assertTrue(!!height2);
    assertTrue(!!height3);
    assertNotEquals(height1, height2);
    assertNotEquals(height2, height3);
  });

  test('restoreFromPrefs extracts style and movement', () => {
    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusMediumCursorWindow, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.MEDIUM_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(
        LineFocusMovement.CURSOR,
        lineFocusController.getCurrentLineFocusMovement());

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusSmallStaticWindow, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.SMALL_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(
        LineFocusMovement.STATIC,
        lineFocusController.getCurrentLineFocusMovement());

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusCursorLine, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.UNDERLINE,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(
        LineFocusMovement.CURSOR,
        lineFocusController.getCurrentLineFocusMovement());
  });

  test('restoreFromPrefs sets enabled', () => {
    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusCursorLine, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusCursorLine, /*isOn=*/ false,
        defaultContainer, defaultHeight);
    assertFalse(lineFocusController.isEnabled());
  });

  test('restoreFromPrefs sets last used line focus mode', () => {
    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusLargeCursorWindow, /*isOn=*/ false,
        defaultContainer, defaultHeight);
    lineFocusController.onKeyDown(toggleKey(), defaultContainer, defaultHeight);

    assertEquals(
        LineFocusStyle.LARGE_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(
        LineFocusMovement.CURSOR,
        lineFocusController.getCurrentLineFocusMovement());
  });

  test('restoreFromPrefs notifies of mode change', () => {
    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusLargeCursorWindow, /*isOn=*/ false,
        defaultContainer, defaultHeight);

    assertTrue(lineFocusModesChanged);
  });

  test('onScrollEnd initiated by line focus, recalculates window', () => {
    const height = 50;
    const scroller = document.createElement('div');
    scroller.style.height = `${height}px`;
    scroller.style.overflow = 'auto';
    const header = document.createElement('h1');
    header.innerText = 'Wicked: For Good';
    const container = document.createElement('p');
    container.style.fontSize = '60px';
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n';
    scroller.appendChild(header);
    scroller.appendChild(container);
    document.body.appendChild(scroller);
    lineFocusController.toggle(true, container, height);
    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, container, height);
    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, container, height);
    lineFocusContentPositionChanged = false;
    lineFocusVisualPositionChanged = false;
    const startingTop = model.getTop();

    lineFocusController.onKeyDown(downKey(), container, height);
    assertFalse(lineFocusContentPositionChanged);
    assertFalse(lineFocusVisualPositionChanged);
    assertEquals(startingTop, model.getTop());

    lineFocusController.onKeyDown(downKey(), container, height);
    lineFocusController.onKeyDown(downKey(), container, height);
    lineFocusController.onScrollEnd(height);
    assertTrue(lineFocusContentPositionChanged);
    assertFalse(lineFocusVisualPositionChanged);
    assertLT(startingTop, model.getTop());
  });

  test('keyboard toggle is logged', async () => {
    const container = createShortContainer();
    lineFocusController.toggle(false, container, defaultHeight);

    lineFocusController.onKeyDown(toggleKey(), container, defaultHeight);
    assertTrue(await metrics.whenCalled('recordLineFocusToggled'));

    metrics.reset();
    lineFocusController.onKeyDown(toggleKey(), container, defaultHeight);
    assertFalse(await metrics.whenCalled('recordLineFocusToggled'));
  });

  test('onMouseMove does nothing if flag disabled', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = false;
    lineFocusContentPositionChanged = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusContentPositionChanged);
  });

  test('onMouseMove does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    readAloudModel.setCurrentTextSegments(
        [{node: ReadAloudNode.create(container)!, start: 0, length: 1}]);
    readAloudModel.setCurrentTextContent('a');
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    speechController.onPlayPauseToggle(container);
    lineFocusContentPositionChanged = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusContentPositionChanged);
  });

  test('onMouseMoveInToolbar does nothing if flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    lineFocusController.onMouseMoveInToolbar(101);

    assertEquals(0, model.getTop());
  });

  test('onMouseMoveInToolbar does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    readAloudModel.setCurrentTextSegments(
        [{node: ReadAloudNode.create(container)!, start: 0, length: 1}]);
    readAloudModel.setCurrentTextContent('a');
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    speechController.onPlayPauseToggle(container);
    const startingTop = model.getTop();

    lineFocusController.onMouseMoveInToolbar(101);

    assertEquals(startingTop, model.getTop());
  });

  test('onAllMenusClose notifies listeners of visual update', () => {
    lineFocusController.toggle(true, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    lineFocusContentPositionChanged = false;
    lineFocusVisualPositionChanged = false;

    lineFocusController.onAllMenusClose();

    assertTrue(lineFocusVisualPositionChanged);
    assertFalse(lineFocusContentPositionChanged);
  });

  test('onKeyDown arrows does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    readAloudModel.setCurrentTextSegments(
        [{node: ReadAloudNode.create(container)!, start: 0, length: 1}]);
    readAloudModel.setCurrentTextContent('a');
    lineFocusController.toggle(true, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    speechController.onPlayPauseToggle(container);
    lineFocusContentPositionChanged = false;

    lineFocusController.onKeyDown(downKey(), container, defaultHeight);
    lineFocusController.onKeyDown(upKey(), container, defaultHeight);

    assertFalse(lineFocusContentPositionChanged);
  });

  test('onKeyDown arrows consumes event with line focus enabled', () => {
    const container = createLongContainer();
    lineFocusController.toggle(true, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusContentPositionChanged = false;

    assertTrue(
        lineFocusController.onKeyDown(downKey(), container, defaultHeight));
    assertTrue(
        lineFocusController.onKeyDown(upKey(), container, defaultHeight));

    assertTrue(lineFocusContentPositionChanged);
  });

  test(
      'onKeyDown arrows does not consume event with line focus disabled',
      () => {
        const container = createLongContainer();
        lineFocusController.toggle(false, container, defaultHeight);
        lineFocusController.onMovementChange(
            LineFocusMovement.CURSOR, container, defaultHeight);
        lineFocusContentPositionChanged = false;

        assertFalse(
            lineFocusController.onKeyDown(downKey(), container, defaultHeight));
        assertFalse(
            lineFocusController.onKeyDown(upKey(), container, defaultHeight));

        assertFalse(lineFocusContentPositionChanged);
      });

  suite('toggle', () => {
    test('first (true) enables line focus', () => {
      lineFocusController.toggle(true, defaultContainer, defaultHeight);

      assertTrue(lineFocusController.isEnabled());
      assertTrue(lineFocusModesChanged);
      assertNotEquals(
          LineFocusType.NONE, lineFocusController.getCurrentLineFocusType());
    });

    test('second (true) enables previously used line focus', () => {
      const previousMode = LineFocusStyle.LARGE_WINDOW;
      // If the default value changes, this test needs to change in order to
      // test the non-default value.
      assertNotEquals(LineFocusStyle.defaultValue(), previousMode);
      lineFocusController.onStyleChange(
          previousMode, defaultContainer, defaultHeight);
      lineFocusController.toggle(false, defaultContainer, defaultHeight);

      lineFocusController.toggle(true, defaultContainer, defaultHeight);

      assertTrue(lineFocusModesChanged);
      assertEquals(
          previousMode.type, lineFocusController.getCurrentLineFocusType());
    });

    test('(false) disables line focus', () => {
      lineFocusController.toggle(true, defaultContainer, defaultHeight);
      lineFocusModesChanged = false;

      lineFocusController.toggle(false, defaultContainer, defaultHeight);

      assertFalse(lineFocusController.isEnabled());
      assertTrue(lineFocusModesChanged);
      assertEquals(
          LineFocusType.NONE, lineFocusController.getCurrentLineFocusType());
      assertEquals(1, metrics.getCallCount('recordLineFocusSession'));
    });

    test('(false) preserves custom style', () => {
      lineFocusController.toggle(true, defaultContainer, defaultHeight);
      lineFocusController.onStyleChange(
          LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
      assertEquals(
          LineFocusStyle.UNDERLINE,
          lineFocusController.getCurrentLineFocusStyle());

      lineFocusController.toggle(false, defaultContainer, defaultHeight);

      assertFalse(lineFocusController.isEnabled());
      assertEquals(
          LineFocusStyle.UNDERLINE,
          lineFocusController.getCurrentLineFocusStyle());
    });

    test('does nothing if already in target state', () => {
      lineFocusController.toggle(true, defaultContainer, defaultHeight);
      lineFocusModesChanged = false;

      lineFocusController.toggle(true, defaultContainer, defaultHeight);

      assertFalse(lineFocusModesChanged);
    });

    test('does nothing if flag is disabled', () => {
      chrome.readingMode.isLineFocusEnabled = false;

      lineFocusController.toggle(true, defaultContainer, defaultHeight);

      assertFalse(lineFocusController.isEnabled());
      assertFalse(lineFocusModesChanged);
    });
  });
});
