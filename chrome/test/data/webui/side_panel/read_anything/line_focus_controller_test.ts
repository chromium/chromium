// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusController, LineFocusMovement, LineFocusStyle, LineFocusType, ReadAloudNode, setInstance, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
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
  let lineFocusMoved: boolean;
  let defaultContainer: HTMLElement;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let readAloudModel: TestReadAloudModelBrowserProxy;
  let metrics: TestMetricsBrowserProxy;
  let keyboardLines: number;
  let speechLines: number;
  let lineFocusToggled: boolean;

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
    return keyDown('l');
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
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    metrics = mockMetrics();
    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    readAloudModel.setInitialized(true);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    lineFocusController = new LineFocusController();
    lineFocusMoved = false;
    lineFocusToggled = false;
    lineFocusListener = {
      onLineFocusMove() {
        lineFocusMoved = true;
      },
      onNeedScrollForLineFocus() {},
      onNeedScrollToTop() {},
      onLineFocusToggled() {
        lineFocusToggled = true;
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
    chrome.readingMode.isLineFocusEnabled = true;
    assertFalse(lineFocusController.isEnabled());
  });

  test('isEnabled is true for line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());
  });

  test('isEnabled is true for window', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());
  });

  test('isEnabled is false for off', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, defaultContainer, defaultHeight);

    assertFalse(lineFocusController.isEnabled());
  });

  test('isEnabled is false with flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    assertFalse(lineFocusController.isEnabled());
  });

  test('onStyleChange updates style only', () => {
    const isStatic = lineFocusController.isStatic();
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.UNDERLINE,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(isStatic, lineFocusController.isStatic());

    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.SMALL_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertEquals(isStatic, lineFocusController.isStatic());

    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.OFF, lineFocusController.getCurrentLineFocusStyle());
    assertEquals(isStatic, lineFocusController.isStatic());
  });

  test('onStyleChange propagates line focus mode', () => {
    chrome.readingMode.isLineFocusEnabled = true;
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

    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusOff,
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
    chrome.readingMode.isLineFocusEnabled = true;
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

  test('onMovementChange when style is off still propagates off', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, defaultContainer, defaultHeight);

    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusOff,
        chrome.readingMode.lastNonDisabledLineFocus);

    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, defaultContainer, defaultHeight);
    assertEquals(
        chrome.readingMode.lineFocusOff,
        chrome.readingMode.lastNonDisabledLineFocus);
  });

  test('onMovementChange updates movement only', () => {
    const startingStyle = lineFocusController.getCurrentLineFocusStyle();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    assertFalse(lineFocusController.isStatic());
    assertEquals(startingStyle, lineFocusController.getCurrentLineFocusStyle());

    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isStatic());
    assertEquals(startingStyle, lineFocusController.getCurrentLineFocusStyle());
  });

  test('onMovementChange to cursor updates position', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    assertEquals(0, lineFocusController.getTop());
  });

  test('onMovementChange to static sets it in the middle', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, defaultContainer, defaultHeight);

    assertEquals(defaultHeight / 2, lineFocusController.getTop());
  });

  test('onStyleChange to window updates position and height', () => {
    const container = createShortContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);

    assertEquals(container.offsetTop, lineFocusController.getTop());
    assertLT(0, lineFocusController.getHeight()!);
  });

  test('onStyleChange window sizes should be different heights', () => {
    const container = createShortContainer();

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    const height1 = lineFocusController.getHeight();
    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, container, defaultHeight);
    const height2 = lineFocusController.getHeight();
    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, container, defaultHeight);
    const height3 = lineFocusController.getHeight();

    assertTrue(!!height1);
    assertTrue(!!height2);
    assertTrue(!!height3);
    assertNotEquals(height1, height2);
    assertNotEquals(height2, height3);
  });

  test('onStyleChange to off after it was enabled logs session', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    assertEquals(1, metrics.getCallCount('recordLineFocusSession'));
  });

  test('onStyleChange to off does nothing if flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    const container = createShortContainer();

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    assertEquals(0, metrics.getCallCount('recordLineFocusSession'));
  });

  test('restoreFromPrefs extracts style and movement', () => {
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusMediumCursorWindow, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.MEDIUM_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertFalse(lineFocusController.isStatic());

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusSmallStaticWindow, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.SMALL_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertTrue(lineFocusController.isStatic());

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusOff, /*isOn=*/ true, defaultContainer,
        defaultHeight);
    assertEquals(
        LineFocusStyle.OFF, lineFocusController.getCurrentLineFocusStyle());
    assertTrue(lineFocusController.isStatic());

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusCursorLine, /*isOn=*/ true,
        defaultContainer, defaultHeight);
    assertEquals(
        LineFocusStyle.UNDERLINE,
        lineFocusController.getCurrentLineFocusStyle());
    assertFalse(lineFocusController.isStatic());
  });

  test('restoreFromPrefs with line focus off, uses previous movement', () => {
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusCursorLine, /*isOn=*/ false,
        defaultContainer, defaultHeight);

    assertEquals(
        LineFocusStyle.OFF, lineFocusController.getCurrentLineFocusStyle());
    assertFalse(lineFocusController.isStatic());
  });

  test('restoreFromPrefs sets last used line focus mode', () => {
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.restoreFromPrefs(
        chrome.readingMode.lineFocusLargeCursorWindow, /*isOn=*/ false,
        defaultContainer, defaultHeight);
    lineFocusController.onKeyDown(toggleKey(), defaultContainer, defaultHeight);

    assertEquals(
        LineFocusStyle.LARGE_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertFalse(lineFocusController.isStatic());
  });

  test('onScrollEnd initiated by line focus, recalculates window', () => {
    chrome.readingMode.isLineFocusEnabled = true;
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
    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, container, height);
    lineFocusController.onStyleChange(
        LineFocusStyle.SMALL_WINDOW, container, height);
    lineFocusMoved = false;
    const startingTop = lineFocusController.getTop();

    lineFocusController.onKeyDown(downKey(), container, height);
    assertFalse(lineFocusMoved);
    assertEquals(startingTop, lineFocusController.getTop());

    lineFocusController.onKeyDown(downKey(), container, height);
    lineFocusController.onKeyDown(downKey(), container, height);
    lineFocusController.onScrollEnd(height);
    assertTrue(lineFocusMoved);
    assertLT(startingTop, lineFocusController.getTop());
  });

  test('toggle while on disables line focus', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);

    lineFocusController.onKeyDown(toggleKey(), container, defaultHeight);

    assertTrue(lineFocusToggled);
    assertEquals(
        LineFocusType.NONE, lineFocusController.getCurrentLineFocusType());
  });

  test('first toggle while off enables default line focus', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    lineFocusController.onKeyDown(toggleKey(), container, defaultHeight);

    assertTrue(lineFocusToggled);
    assertEquals(
        LineFocusStyle.defaultValue(),
        lineFocusController.getCurrentLineFocusStyle());
  });

  test('toggle while off enables previously used line focus', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    const previousMode = LineFocusStyle.LARGE_WINDOW;
    // If the default value changes, this test needs to change in order to test
    // the non-default value.
    assertNotEquals(LineFocusStyle.defaultValue(), previousMode);
    lineFocusController.onStyleChange(previousMode, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    lineFocusController.onKeyDown(toggleKey(), container, defaultHeight);

    assertTrue(lineFocusToggled);
    assertEquals(
        previousMode.type, lineFocusController.getCurrentLineFocusType());
  });

  test('toggle is logged', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

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
    lineFocusMoved = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusMoved);
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
    chrome.readingMode.isLineFocusEnabled = true;
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusMoved);
  });

  test('onMouseMoveInToolbar does nothing if flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);

    lineFocusController.onMouseMoveInToolbar(101);

    assertEquals(0, lineFocusController.getTop());
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
    chrome.readingMode.isLineFocusEnabled = true;
    speechController.onPlayPauseToggle(container);
    const startingTop = lineFocusController.getTop();

    lineFocusController.onMouseMoveInToolbar(101);

    assertEquals(startingTop, lineFocusController.getTop());
  });

  test('onAllMenusClose notifies listeners', () => {
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusMoved = false;

    lineFocusController.onAllMenusClose();

    assertTrue(lineFocusMoved);
  });

  test('onKeyDown arrows does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    readAloudModel.setCurrentTextSegments(
        [{node: ReadAloudNode.create(container)!, start: 0, length: 1}]);
    readAloudModel.setCurrentTextContent('a');
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.onKeyDown(downKey(), container, defaultHeight);
    lineFocusController.onKeyDown(upKey(), container, defaultHeight);

    assertFalse(lineFocusMoved);
  });

  test('onKeyDown arrows consumes event with line focus enabled', () => {
    const container = createLongContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusMoved = false;

    assertTrue(
        lineFocusController.onKeyDown(downKey(), container, defaultHeight));
    assertTrue(
        lineFocusController.onKeyDown(upKey(), container, defaultHeight));

    assertTrue(lineFocusMoved);
  });

  test(
      'onKeyDown arrows does not consume event with line focus disabled',
      () => {
        const container = createLongContainer();
        lineFocusController.onStyleChange(
            LineFocusStyle.OFF, container, defaultHeight);
        lineFocusController.onMovementChange(
            LineFocusMovement.CURSOR, container, defaultHeight);
        chrome.readingMode.isLineFocusEnabled = true;
        lineFocusMoved = false;

        assertFalse(
            lineFocusController.onKeyDown(downKey(), container, defaultHeight));
        assertFalse(
            lineFocusController.onKeyDown(upKey(), container, defaultHeight));

        assertFalse(lineFocusMoved);
      });
});
