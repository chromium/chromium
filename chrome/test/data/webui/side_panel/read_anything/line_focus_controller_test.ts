// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusController, LineFocusMovement, LineFocusStyle, LineFocusType, NodeStore, ReadAloudNode, setInstance, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineFocusListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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
  let scrollDiffReceived: number;
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
    document.body.appendChild(container);
    return container;
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
    scrollDiffReceived = 0;
    lineFocusToggled = false;
    lineFocusListener = {
      onLineFocusMove() {
        lineFocusMoved = true;
      },

      onNeedScrollForLineFocus(scrollDiff: number) {
        scrollDiffReceived = scrollDiff;
      },

      onNeedScrollToTop() {},
      onLineFocusToggled() {
        lineFocusToggled = true;
      },
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

  test('onStyleChange to different mode does not restart session', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);
    let started = false;
    chrome.readingMode.startLineFocusSession = () => started = true;
    metrics.reset();

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    assertTrue(started);

    started = false;
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    assertFalse(started);

    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);
    assertEquals(1, metrics.getCallCount('recordLineFocusSession'));
  });

  test('onMovementChange to different mode does not restart session', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    let started = false;
    chrome.readingMode.startLineFocusSession = () => started = true;
    metrics.reset();

    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    assertFalse(started);
  });

  test('onStyleChange to off resets position and height', () => {
    const container = createShortContainer();

    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    assertEquals(0, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
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
    lineFocusController.toggle(defaultContainer, defaultHeight);

    assertEquals(
        LineFocusStyle.LARGE_WINDOW,
        lineFocusController.getCurrentLineFocusStyle());
    assertFalse(lineFocusController.isStatic());
  });

  test('onScrollEnd adds scroll distance', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    let scrollDistance = 0;
    let mouseDistance = 0;
    chrome.readingMode.addLineFocusScrollDistance = y => {
      scrollDistance = y;
    };
    chrome.readingMode.addLineFocusMouseDistance = y => {
      mouseDistance = y;
    };
    const top1 = 43;
    const top2 = 55;
    const top3 = 12;
    // Ensure we test scrolling up and down;
    assertLT(top1, top2);
    assertGT(top2, top3);

    lineFocusController.onScrollEnd(top1);
    assertEquals(0, mouseDistance);
    assertEquals(top1, scrollDistance);

    lineFocusController.onScrollEnd(top2);
    assertEquals(0, mouseDistance);
    assertEquals(top2 - top1, scrollDistance);

    lineFocusController.onScrollEnd(top3);
    assertEquals(0, mouseDistance);
    assertEquals(top2 - top3, scrollDistance);
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

    lineFocusController.snapToNextLine(true);
    assertFalse(lineFocusMoved);
    assertEquals(startingTop, lineFocusController.getTop());

    lineFocusController.snapToNextLine(true);
    lineFocusController.snapToNextLine(true);
    lineFocusController.onScrollEnd(height);
    assertTrue(lineFocusMoved);
    assertLT(startingTop, lineFocusController.getTop());
  });

  test('onMouseMove adds mouse distance', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    let scrollDistance = 0;
    let mouseDistance = 0;
    chrome.readingMode.addLineFocusScrollDistance = y => {
      scrollDistance = y;
    };
    chrome.readingMode.addLineFocusMouseDistance = y => {
      mouseDistance = y;
    };
    const y1 = 43;
    const y2 = 55;
    const y3 = 12;
    // Ensure we test moving up and down;
    assertLT(y1, y2);
    assertGT(y2, y3);

    lineFocusController.onMouseMove(y1);
    assertEquals(y1, mouseDistance);
    assertEquals(0, scrollDistance);

    lineFocusController.onMouseMove(y2);
    assertEquals(y2 - y1, mouseDistance);
    assertEquals(0, scrollDistance);

    lineFocusController.onMouseMove(y3);
    assertEquals(y2 - y3, mouseDistance);
    assertEquals(0, scrollDistance);
  });

  test('toggle while on disables line focus', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);

    lineFocusController.toggle(container, defaultHeight);

    assertTrue(lineFocusToggled);
    assertEquals(
        LineFocusType.NONE, lineFocusController.getCurrentLineFocusType());
  });

  test('first toggle while off enables default line focus', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    lineFocusController.toggle(container, defaultHeight);

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

    lineFocusController.toggle(container, defaultHeight);

    assertTrue(lineFocusToggled);
    assertEquals(
        previousMode.type, lineFocusController.getCurrentLineFocusType());
  });

  test('toggle is logged', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);

    lineFocusController.toggle(container, defaultHeight);
    assertTrue(await metrics.whenCalled('recordLineFocusToggled'));

    metrics.reset();
    lineFocusController.toggle(container, defaultHeight);
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

  test('onMouseMove notifies listeners', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusMoved = false;

    lineFocusController.onMouseMove(101);

    assertTrue(lineFocusMoved);
  });

  test('onMouseMove does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
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

  test('onMouseMove does nothing when line is static', () => {
    const container = createLongContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusMoved = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusMoved);
  });

  test('onMouseMove sets new line position', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const newPos = 102;

    lineFocusController.onMouseMove(newPos);

    assertEquals(newPos, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMove honors container top with line', () => {
    const container = createShortContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMove(0);

    assertLT(0, container.offsetTop);
    assertEquals(container.offsetTop, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMove sets new window position and height', () => {
    const container = createLongContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const newPos = container.offsetTop + 50;

    lineFocusController.onMouseMove(newPos);

    const height = lineFocusController.getHeight();
    assertTrue(!!height);
    assertLT(0, height);
    // The window should be approximately centered around the mouse position.
    assertGT(newPos, lineFocusController.getTop());
    assertLT(newPos, lineFocusController.getTop() + height);
  });

  test('onMouseMove honors container top with height', () => {
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMove(0);

    assertLT(0, container.offsetTop);
    assertEquals(container.offsetTop, lineFocusController.getTop());
    const height = lineFocusController.getHeight();
    assertTrue(!!height);
    assertLT(0, height);
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

  test('onMouseMoveInToolbar does not notify listeners', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusMoved = false;

    lineFocusController.onMouseMoveInToolbar(101);

    assertFalse(lineFocusMoved);
  });

  test('onMouseMoveInToolbar does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
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

  test('onMouseMoveInToolbar does nothing when line is static', () => {
    const container = createLongContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const startingTop = lineFocusController.getTop();

    lineFocusController.onMouseMoveInToolbar(101);

    assertEquals(startingTop, lineFocusController.getTop());
  });

  test('onMouseMoveInToolbar sets new line position', () => {
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, defaultContainer, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const newPos = 102;

    lineFocusController.onMouseMoveInToolbar(newPos);

    assertEquals(newPos, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMoveInToolbar honors container top with line', () => {
    const container = createShortContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMoveInToolbar(0);

    assertLT(0, container.offsetTop);
    assertEquals(container.offsetTop, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMoveInToolbar sets new window position and height', () => {
    const container = createLongContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const newPos = container.offsetTop + 50;

    lineFocusController.onMouseMoveInToolbar(newPos);

    const height = lineFocusController.getHeight();
    assertTrue(!!height);
    assertLT(0, height);
    assertGT(newPos, lineFocusController.getTop());
    assertLT(newPos, lineFocusController.getTop() + height);
  });

  test('onMouseMoveInToolbar honors container top with height', () => {
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.LARGE_WINDOW, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMoveInToolbar(0);

    assertLT(0, container.offsetTop);
    assertEquals(container.offsetTop, lineFocusController.getTop());
    const height = lineFocusController.getHeight();
    assertTrue(!!height);
    assertLT(0, height);
  });

  test('onAllMenusClose notifies listeners', () => {
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusMoved = false;

    lineFocusController.onAllMenusClose();

    assertTrue(lineFocusMoved);
  });

  test('onTextLocationsChange updates window position and height', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    const oldTop = lineFocusController.getTop();
    const oldHeight = lineFocusController.getHeight();
    const heading = document.createElement('h1');
    heading.innerText =
        'Like a comet pulled from orbit as\n\n\n\n\nas it passes the sun\n' +
        'Like a stream that meets a boulder\n\nhalfway through the woods';
    document.body.appendChild(heading);
    lineFocusMoved = false;

    lineFocusController.onTextLocationsChange(heading, defaultHeight);

    assertNotEquals(NaN, lineFocusController.getTop());
    assertNotEquals(NaN, lineFocusController.getHeight());
    assertNotEquals(oldTop, lineFocusController.getTop());
    assertNotEquals(oldHeight, lineFocusController.getHeight());
    assertTrue(lineFocusMoved);
  });

  test('onTextLocationsChange keeps line position', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createLongContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusController.onMouseMove(100);
    const oldTop = lineFocusController.getTop();
    const heading = document.createElement('h1');
    heading.innerText =
        'Who can say if I\'ve been changed for the better\n\n\n\n\nBut\n' +
        'Because I knew you\n\nI have been changed for good';
    document.body.appendChild(heading);

    lineFocusController.onTextLocationsChange(heading, defaultHeight);

    assertEquals(oldTop, lineFocusController.getTop());
  });

  test('onWordBoundary updates position', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusMoved = false;
    NodeStore.getInstance().setDomNode(container, 1);
    const segments = [{
      node: ReadAloudNode.create(container)!,
      start: 0,
      length: 5,
    }];

    lineFocusController.onWordBoundary(segments);

    assertTrue(lineFocusMoved);
    assertEquals(1, speechLines);
    assertEquals(0, keyboardLines);
  });

  test('onWordBoundary only counts new lines', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusMoved = false;
    NodeStore.getInstance().setDomNode(container, 1);
    const segments1 = [{
      node: ReadAloudNode.create(container)!,
      start: 0,
      length: 5,
    }];
    const segments2 = [{
      node: ReadAloudNode.create(container)!,
      start: 5,
      length: 5,
    }];

    lineFocusController.onWordBoundary(segments1);
    assertEquals(1, speechLines);

    lineFocusController.onWordBoundary(segments2);
    assertEquals(1, speechLines);
  });

  test('onWordBoundary scrolls with static line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createLongContainer();
    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, container, 100);
    lineFocusController.onStyleChange(LineFocusStyle.UNDERLINE, container, 100);
    lineFocusMoved = false;
    NodeStore.getInstance().setDomNode(container, 1);
    const segments = [{
      node: ReadAloudNode.create(container)!,
      start: 200,
      length: 5,
    }];

    lineFocusController.onWordBoundary(segments);

    assertLT(0, scrollDiffReceived);
    assertFalse(lineFocusMoved);
  });

  test('onWordBoundary does nothing when disabled', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.OFF, container, defaultHeight);
    assertFalse(lineFocusController.isEnabled());
    lineFocusMoved = false;
    scrollDiffReceived = 0;
    NodeStore.getInstance().setDomNode(container, 1);
    const segments = [{
      node: ReadAloudNode.create(container)!,
      start: 0,
      length: 5,
    }];

    lineFocusController.onWordBoundary(segments);

    assertFalse(lineFocusMoved);
    assertEquals(0, speechLines);
    assertEquals(0, keyboardLines);
    assertEquals(0, scrollDiffReceived);
  });

  test('snapToNextLine with cursor line moves by line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = document.createElement('p');
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood';
    document.body.appendChild(container);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    let oldTop = lineFocusController.getTop();

    // Snap to the first line.
    lineFocusController.snapToNextLine(true);
    let newTop = lineFocusController.getTop();
    assertLT(oldTop, newTop);
    assertEquals(1, keyboardLines);

    // Snap to the last line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true);
    newTop = lineFocusController.getTop();
    assertLT(oldTop, newTop);
    assertEquals(2, keyboardLines);

    // The container only has two lines, so moving forward should not change
    // position.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertEquals(2, keyboardLines);

    // Snap back to the first line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false);
    newTop = lineFocusController.getTop();
    assertGT(oldTop, newTop);
    assertEquals(3, keyboardLines);

    // Moving back again should not change position.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertEquals(3, keyboardLines);
    assertEquals(0, speechLines);
  });

  test('snapToNextLine with static line scrolls by line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createLongContainer();
    container.style.fontSize = '60px';
    lineFocusController.onStyleChange(LineFocusStyle.UNDERLINE, container, 50);
    lineFocusController.onMovementChange(
        LineFocusMovement.STATIC, container, 100);
    let oldTop = lineFocusController.getTop();
    let oldScrollDiff = scrollDiffReceived;

    // Snap to the first line.
    lineFocusController.snapToNextLine(true);
    let newTop = lineFocusController.getTop();
    let newScrollDiff = scrollDiffReceived;
    assertEquals(oldTop, newTop);
    assertLT(oldScrollDiff, newScrollDiff);
    assertEquals(1, keyboardLines);

    // Snap to the last line.
    oldTop = newTop;
    oldScrollDiff = newScrollDiff;
    lineFocusController.snapToNextLine(true);
    newTop = lineFocusController.getTop();
    newScrollDiff = scrollDiffReceived;
    assertEquals(oldTop, newTop);
    assertLT(oldScrollDiff, newScrollDiff);
    assertEquals(2, keyboardLines);

    // Snap back to the first line.
    oldTop = newTop;
    oldScrollDiff = newScrollDiff;
    lineFocusController.snapToNextLine(false);
    newTop = lineFocusController.getTop();
    newScrollDiff = scrollDiffReceived;
    assertEquals(oldTop, newTop);
    assertGT(oldScrollDiff, newScrollDiff);
    assertEquals(3, keyboardLines);
    assertEquals(0, speechLines);
  });

  test('snapToNextLine scrolls down to line if out of view', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const height = 100;
    const scroller = document.createElement('div');
    scroller.style.height = `${height}px`;
    scroller.style.overflow = 'auto';
    const container = document.createElement('p');
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n';
    scroller.appendChild(container);
    document.body.appendChild(scroller);
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, height);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, height);
    let oldTop = lineFocusController.getTop();

    // Snap to the first line.
    lineFocusController.snapToNextLine(true);
    let newTop = lineFocusController.getTop();
    // Continue moving to the next line until scrolling occurs.
    while (oldTop < newTop) {
      assertEquals(0, scrollDiffReceived);
      oldTop = newTop;
      lineFocusController.snapToNextLine(true);
      newTop = lineFocusController.getTop();
    }

    assertLT(0, scrollDiffReceived);
    assertLT(0, keyboardLines);
    assertEquals(0, speechLines);
  });

  test('snapToNextLine after user scroll uses current position', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const height = 100;
    const scroller = document.createElement('div');
    scroller.style.height = `${height}px`;
    scroller.style.overflow = 'auto';
    const container = document.createElement('p');
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n';
    scroller.appendChild(container);
    document.body.appendChild(scroller);
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, height);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, height);

    lineFocusController.snapToNextLine(true);
    lineFocusController.onScrollEnd(height);
    lineFocusController.snapToNextLine(true);

    // The window is 3 lines high. If the line index was kept then the second
    // snapToNextLine call would move by one line, and this would be 4. But
    // after the scroll, the current line index has to be recalculated and so
    // all 3 of the new highlighted lines are counted.
    assertEquals(6, keyboardLines);
  });

  test('snapToNextLine scrolls up to line if out of view', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const height = 100;
    const scroller = document.createElement('div');
    scroller.style.height = `${height}px`;
    scroller.style.overflow = 'auto';
    const container = document.createElement('p');
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n';
    scroller.appendChild(container);
    document.body.appendChild(scroller);
    scroller.scrollTop = 10000;
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, height);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, height);
    let oldTop = lineFocusController.getTop();

    // Snap to the first line.
    lineFocusController.snapToNextLine(false);
    let newTop = lineFocusController.getTop();
    // Continue moving to the previous line until scrolling occurs.
    while (oldTop > newTop) {
      assertEquals(0, scrollDiffReceived);
      oldTop = newTop;
      lineFocusController.snapToNextLine(false);
      newTop = lineFocusController.getTop();
    }

    assertGT(0, scrollDiffReceived);
    assertLT(0, keyboardLines);
    assertEquals(0, speechLines);
  });

  test('snapToNextLine with window moves by line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = document.createElement('p');
    container.innerText = 'Who can say if I\'ve been changed for the better\n' +
        'But Because I knew you I have been changed for good\n' +
        'And just to clear the air I ask forgiveness\n' +
        'for the things I\'ve done you blame before';
    document.body.appendChild(container);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    lineFocusController.onStyleChange(
        LineFocusStyle.MEDIUM_WINDOW, container, defaultHeight);
    let oldTop = lineFocusController.getTop();

    // Snap to the second line.
    lineFocusController.snapToNextLine(true);
    let newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertEquals(3, keyboardLines);

    // Snap to the last line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true);
    newTop = lineFocusController.getTop();
    assertLT(oldTop, newTop);
    assertEquals(4, keyboardLines);

    // Moving forward should not change position.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertEquals(4, keyboardLines);

    // Snap back to the second line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false);
    newTop = lineFocusController.getTop();
    assertGT(oldTop, newTop);
    assertEquals(5, keyboardLines);

    // Moving back again should not change position since the window is 3 lines
    // long and we are already surrounding the second line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertEquals(5, keyboardLines);
    assertEquals(0, speechLines);
  });

  test('snapToNextLine does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    lineFocusController.onStyleChange(
        LineFocusStyle.UNDERLINE, container, defaultHeight);
    lineFocusController.onMovementChange(
        LineFocusMovement.CURSOR, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.snapToNextLine(true);
    lineFocusController.snapToNextLine(false);

    assertFalse(lineFocusMoved);
  });
});
