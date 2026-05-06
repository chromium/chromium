// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusCursorMoveMode, LineFocusLineStyleMode, LineFocusModel, LineFocusMovement, LineFocusNoneMoveMode, LineFocusStaticMoveMode, LineFocusStyle, LineFocusWindowStyleMode, NodeStore, ReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineFocusMoveMode, MoveModeDelegate} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('LineFocusMoveMode', () => {
  let model: LineFocusModel;
  let styleMode: LineFocusLineStyleMode;
  let windowMode: LineFocusWindowStyleMode;
  let delegate: MoveModeDelegate;
  let sessionEnded: boolean;
  let notifiedMove: boolean;
  let scrollDiffReceived: number;
  let bufferValReceived: boolean|undefined;
  let speechLines: number;
  let keyboardLines: number;

  const defaultHeight = 1000;

  function createShortContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'I\'ve heard it said\nThat people come into our lives\nfor a reason.';
    container.style.whiteSpace = 'pre-line';
    document.body.appendChild(container);
    return container;
  }

  function mockLinesCounters() {
    speechLines = 0;
    keyboardLines = 0;
    chrome.readingMode.incrementLineFocusSpeechLines = () => speechLines++;
    chrome.readingMode.incrementLineFocusKeyboardLines = () => keyboardLines++;
  }

  function snapForward(mode: LineFocusMoveMode): void {
    mode.snapToNextLine(/*isForward=*/ true);
  }

  function snapBackward(mode: LineFocusMoveMode): void {
    mode.snapToNextLine(/*isForward=*/ false);
  }

  function setDefaultTextBounds(): void {
    const rect1 = new DOMRect(0, 10, 100, 20);
    const rect2 = new DOMRect(0, 30, 100, 20);
    const rect3 = new DOMRect(0, 50, 100, 20);
    model.setTextBounds([rect1, rect2, rect3]);
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    // Initialize font size so that the threshold for merging text bounds
    // is correctly calculated and not zero.
    readingMode.fontSize = 20;
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    model = new LineFocusModel();
    styleMode = new LineFocusLineStyleMode(LineFocusStyle.UNDERLINE, model);
    windowMode =
        new LineFocusWindowStyleMode(LineFocusStyle.MEDIUM_WINDOW, model);
    sessionEnded = false;
    notifiedMove = false;
    scrollDiffReceived = 0;
    bufferValReceived = undefined;
    delegate = {
      onSessionEnd() {
        sessionEnded = true;
      },
      notifyMove() {
        notifiedMove = true;
      },
      notifyScroll(diff) {
        scrollDiffReceived = diff;
      },
      notifyScrollToTop() {},
      notifyScrollBuffer(buffer) {
        bufferValReceived = buffer;
      },
    };
  });

  suite('static mode', () => {
    let mode: LineFocusStaticMoveMode;

    setup(() => {
      mode = new LineFocusStaticMoveMode(model, styleMode, delegate);
    });

    test('getMovement returns STATIC', () => {
      assertEquals(LineFocusMovement.STATIC, mode.getMovement());
    });

    test('onActivated starts session', () => {
      model.setSessionActive(false);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertTrue(started);
      assertTrue(model.isSessionActive());
      assertEquals(styleMode.getStyle(), model.getLastEnabledLineFocusStyle());
    });

    test('onActivated should not adapt multi-line window', () => {
      const container = document.createElement('div');
      mode.onActivated(container, defaultHeight);
      assertFalse(model.getAdaptMultiLineWindow());
    });

    test('onActivated updates positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onActivated sets center focal point', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      const expectedFocalPoint = model.getMaxY() / 2;
      assertEquals(expectedFocalPoint, model.getFocalPoint());
      assertTrue(notifiedMove);
    });

    test('onActivated notifies delegate of scroll buffer', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      // Need to do a direct compare since the starting value is undefined.
      assertTrue(bufferValReceived === true);
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertFalse(started);
      assertTrue(model.isSessionActive());
    });

    test('onWordBoundary scrolls to line', () => {
      const container = createShortContainer();
      model.setMaxY(defaultHeight * 2);
      NodeStore.getInstance().setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 7,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertFalse(notifiedMove);
      assertTrue(model.getInitiatedScroll());
    });

    test('onWordBoundary scrolls to line if it would go off screen', () => {
      const container = createShortContainer();
      NodeStore.getInstance().setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 7,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertFalse(notifiedMove);
      assertTrue(model.getInitiatedScroll());
    });

    test('onWordBoundary only counts new lines', () => {
      const container = createShortContainer();
      mockLinesCounters();
      NodeStore.getInstance().setDomNode(container, 1);
      const segments1 = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];
      const segments2 = [{
        node: ReadAloudNode.create(container)!,
        start: 5,
        length: 3,
      }];

      mode.onWordBoundary(segments1);
      assertLT(0, scrollDiffReceived);
      assertEquals(1, speechLines);

      // Mock the panel scroll so the next segment is on the same line.
      model.setFocalPoint(model.getFocalPoint() + scrollDiffReceived);
      mode.onWordBoundary(segments2);
      assertLT(0, scrollDiffReceived);
      assertEquals(1, speechLines);
    });

    test('onMouseMove does nothing', () => {
      mode.onMouseMove(101);
      assertFalse(notifiedMove);
    });

    test('onMouseMoveInToolbar does nothing', () => {
      mode.onMouseMoveInToolbar(101);
      assertFalse(notifiedMove);
    });

    test('onScrollEnd adds scroll distance', () => {
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

      mode.onScrollEnd(top1);
      assertEquals(0, mouseDistance);
      assertEquals(top1, scrollDistance);

      mode.onScrollEnd(top2);
      assertEquals(0, mouseDistance);
      assertEquals(top2 - top1, scrollDistance);

      mode.onScrollEnd(top3);
      assertEquals(0, mouseDistance);
      assertEquals(top2 - top3, scrollDistance);
    });

    test(
        'onScrollEnd notifies of move for line focus scroll for small window only',
        () => {
          const rect1 = new DOMRect(0, 10, 100, 20);
          const rect2 = new DOMRect(0, 30, 100, 20);
          const rect3 = new DOMRect(0, 50, 100, 20);
          model.setTextBounds([rect1, rect2, rect3]);
          model.setCurrentLineIndex(1);
          model.setTop(10);
          model.setWindowHeight(10);
          const singleWindow =
              new LineFocusWindowStyleMode(LineFocusStyle.SMALL_WINDOW, model);
          model.setInitiatedScroll(true);

          mode.onScrollEnd(100);
          assertFalse(notifiedMove);

          mode = new LineFocusStaticMoveMode(model, singleWindow, delegate);
          mode.onScrollEnd(100);
          assertTrue(notifiedMove);
        });

    test('onScrollEnd notifies of move for user scroll', () => {
      model.setInitiatedScroll(false);
      mode.onScrollEnd(100);

      assertTrue(notifiedMove);
    });

    test('onScrollEnd notifies of move for single-line window only', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);
      model.setCurrentLineIndex(1);
      model.setTop(10);
      model.setWindowHeight(10);

      model.setInitiatedScroll(true);
      mode = new LineFocusStaticMoveMode(model, windowMode, delegate);
      mode.onScrollEnd(100);
      assertFalse(notifiedMove);

      model.setInitiatedScroll(true);
      const singleWindow =
          new LineFocusWindowStyleMode(LineFocusStyle.SMALL_WINDOW, model);
      mode = new LineFocusStaticMoveMode(model, singleWindow, delegate);
      mode.onScrollEnd(100);
      assertTrue(notifiedMove);
    });

    test('onTextLocationsChange updates scroll buffer', () => {
      const container = createShortContainer();
      mode.onTextLocationsChange(container, defaultHeight);
      assertEquals(true, bufferValReceived);
    });

    test('onTextLocationsChange updates positions', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onTextLocationsChange re-centers focal point', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertTrue(notifiedMove);
      const center = model.getMaxY() / 2;
      assertEquals(center, model.getFocalPoint());
    });

    test('snapToNextLine scrolls by line', () => {
      mockLinesCounters();
      setDefaultTextBounds();
      model.setMaxY(defaultHeight);
      let oldTop = model.getTop();
      let oldScrollDiff = scrollDiffReceived;

      // Snap to the first line.
      snapForward(mode);
      let newTop = model.getTop();
      let newScrollDiff = scrollDiffReceived;
      assertEquals(0, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertLT(oldScrollDiff, newScrollDiff);
      assertEquals(1, keyboardLines);

      // Snap to the second line.
      snapForward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(1, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertLT(oldScrollDiff, newScrollDiff);
      assertEquals(2, keyboardLines);

      // Snap to the last line.
      oldTop = newTop;
      oldScrollDiff = newScrollDiff;
      snapForward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(2, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertLT(oldScrollDiff, newScrollDiff);
      assertEquals(3, keyboardLines);

      // Snap back to the second line.
      oldTop = newTop;
      oldScrollDiff = newScrollDiff;
      snapBackward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(1, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertGT(oldScrollDiff, newScrollDiff);
      assertEquals(4, keyboardLines);

      // Snap back to the first line.
      oldTop = newTop;
      oldScrollDiff = newScrollDiff;
      snapBackward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(0, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertGT(oldScrollDiff, newScrollDiff);
      assertEquals(5, keyboardLines);
      assertEquals(0, speechLines);
    });

    test('snapToNextLine returns true with text bounds', () => {
      mockLinesCounters();
      model.setMaxY(defaultHeight);

      assertFalse(mode.snapToNextLine(true));
      assertFalse(mode.snapToNextLine(false));

      setDefaultTextBounds();
      assertTrue(mode.snapToNextLine(true));
      assertTrue(mode.snapToNextLine(false));
    });
  });

  suite('cursor mode', () => {
    let mode: LineFocusCursorMoveMode;

    function createWindowMode(): LineFocusCursorMoveMode {
      return new LineFocusCursorMoveMode(model, windowMode, delegate);
    }

    setup(() => {
      mode = new LineFocusCursorMoveMode(model, styleMode, delegate);
      setDefaultTextBounds();
      model.setAdaptMultiLineWindow(true);
    });

    test('getMovement returns CURSOR', () => {
      assertEquals(LineFocusMovement.CURSOR, mode.getMovement());
    });

    test('onActivated starts session', () => {
      model.setSessionActive(false);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertTrue(started);
      assertTrue(model.isSessionActive());
      assertEquals(styleMode.getStyle(), model.getLastEnabledLineFocusStyle());
    });

    test('onActivated should adapt multi-line window', () => {
      model.setAdaptMultiLineWindow(false);
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertTrue(model.getAdaptMultiLineWindow());
    });

    test('onActivated updates positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onActivated from off moves to first line', () => {
      const container = createShortContainer();
      model.setSessionActive(false);

      mode.onActivated(container, defaultHeight);

      assertLT(model.getMinY(), model.getFocalPoint());
      assertEquals(0, model.getCurrentLineIndex());
      assertTrue(notifiedMove);
    });

    test('onActivated from on does not move to first line', () => {
      const container = createShortContainer();
      model.setSessionActive(true);

      mode.onActivated(container, defaultHeight);

      assertEquals(model.getMinY(), model.getFocalPoint());
      assertEquals(null, model.getCurrentLineIndex());
      assertTrue(notifiedMove);
    });

    test('onActivated notifies delegate of no scroll buffer', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      // Need to do a direct compare since the starting value is undefined.
      assertTrue(bufferValReceived === false);
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertFalse(started);
      assertTrue(model.isSessionActive());
    });

    test('onWordBoundary updates position', () => {
      const container = createShortContainer();
      model.setMaxY(defaultHeight * 2);
      NodeStore.getInstance().setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertEquals(0, scrollDiffReceived);
      assertTrue(notifiedMove);
    });

    test('onWordBoundary scrolls if line would go off screen', () => {
      const container = createShortContainer();
      model.setMaxY(10);
      NodeStore.getInstance().setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertTrue(notifiedMove);
      assertTrue(model.getInitiatedScroll());
    });

    test('onWordBoundary only counts new lines', () => {
      const container = createShortContainer();
      mockLinesCounters();
      NodeStore.getInstance().setDomNode(container, 1);
      const segments1 = [{
        node: ReadAloudNode.create(container)!,
        start: 5,
        length: 5,
      }];
      const segments2 = [{
        node: ReadAloudNode.create(container)!,
        start: 12,
        length: 3,
      }];

      mode.onWordBoundary(segments1);
      assertEquals(1, speechLines);

      mode.onWordBoundary(segments2);
      assertEquals(1, speechLines);
    });

    test('onMouseMove adds mouse distance', () => {
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

      mode.onMouseMove(y1);
      assertEquals(y1, mouseDistance);
      assertEquals(0, scrollDistance);

      mode.onMouseMove(y2);
      assertEquals(y2 - y1, mouseDistance);
      assertEquals(0, scrollDistance);

      mode.onMouseMove(y3);
      assertEquals(y2 - y3, mouseDistance);
      assertEquals(0, scrollDistance);
    });

    test('onMouseMove notifies listeners', () => {
      mode.onMouseMove(101);
      assertTrue(notifiedMove);
    });

    test('onMouseMove sets new line position', () => {
      const newPos = 102;

      mode.onMouseMove(newPos);

      assertEquals(newPos, model.getFocalPoint());
      assertEquals(newPos, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onMouseMove honors min y with line', () => {
      const minY = 10;
      model.setMinY(minY);

      mode.onMouseMove(0);

      assertEquals(minY, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onMouseMove sets new window position and height', () => {
      mode = createWindowMode();
      const minY = 10;
      model.setMinY(minY);
      const newPos = minY + 50;

      mode.onMouseMove(newPos);

      assertEquals(newPos, model.getFocalPoint());
      assertLT(0, model.getWindowHeight());
      // The window should be approximately centered around the mouse position.
      assertGT(newPos, model.getTop());
      assertLT(newPos, model.getTop() + model.getWindowHeight());
    });

    test('onMouseMove honors min y with window', () => {
      mode = createWindowMode();
      const minY = 10;
      model.setMinY(minY);

      mode.onMouseMove(0);

      assertEquals(minY, model.getTop());
      assertLT(0, model.getWindowHeight());
    });

    test('onMouseMoveInToolbar does not notify listeners', () => {
      mode.onMouseMoveInToolbar(101);
      assertFalse(notifiedMove);
    });

    test('onMouseMoveInToolbar sets new line position', () => {
      const newPos = 102;

      mode.onMouseMoveInToolbar(newPos);

      assertEquals(newPos, model.getFocalPoint());
      assertEquals(newPos, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onMouseMoveInToolbar honors min y with line', () => {
      const minY = 10;
      model.setMinY(minY);

      mode.onMouseMoveInToolbar(0);

      assertEquals(minY, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onMouseMoveInToolbar sets new window position and height', () => {
      mode = createWindowMode();
      const minY = 10;
      model.setMinY(minY);
      const newPos = minY + 50;

      mode.onMouseMoveInToolbar(newPos);

      assertEquals(newPos, model.getFocalPoint());
      assertLT(0, model.getWindowHeight());
      // The window should be approximately centered around the mouse position.
      assertGT(newPos, model.getTop());
      assertLT(newPos, model.getTop() + model.getWindowHeight());
    });

    test('onMouseMoveInToolbar honors min y with window', () => {
      mode = createWindowMode();
      const minY = 10;
      model.setMinY(minY);

      mode.onMouseMoveInToolbar(0);

      assertEquals(minY, model.getTop());
      assertLT(0, model.getWindowHeight());
    });

    test('onScrollEnd adds scroll distance', () => {
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

      mode.onScrollEnd(top1);
      assertEquals(0, mouseDistance);
      assertEquals(top1, scrollDistance);

      mode.onScrollEnd(top2);
      assertEquals(0, mouseDistance);
      assertEquals(top2 - top1, scrollDistance);

      mode.onScrollEnd(top3);
      assertEquals(0, mouseDistance);
      assertEquals(top2 - top3, scrollDistance);
    });

    test('onTextLocationsChange scrolls to re-center line focus', () => {
      const container = createShortContainer();
      mode.onTextLocationsChange(container, 10);
      assertNotEquals(0, scrollDiffReceived);
    });

    test('onTextLocationsChange updates scroll buffer', () => {
      const container = createShortContainer();
      mode.onTextLocationsChange(container, defaultHeight);
      assertEquals(false, bufferValReceived);
    });

    test('onTextLocationsChange updates positions', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onTextLocationsChange moves to new focal point', () => {
      const container = createShortContainer();
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);
      model.setCurrentLineIndex(0);

      mode.onTextLocationsChange(container, defaultHeight);

      assertLT(0, model.getFocalPoint());
      assertTrue(notifiedMove);
    });

    test('onTextLocationsChange moves to new focal point with window', () => {
      const container = createShortContainer();
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);
      model.setCurrentLineIndex(1);
      mode = createWindowMode();

      mode.onTextLocationsChange(container, defaultHeight);

      const focalPoint = model.getFocalPoint();
      assertLT(0, focalPoint);
      assertGT(focalPoint, model.getTop());
      assertLT(0, model.getWindowHeight());
      assertLT(focalPoint, model.getTop() + model.getWindowHeight());
      assertTrue(notifiedMove);
    });

    test('snapToNextLine moves by line', () => {
      mockLinesCounters();
      model.setMaxY(defaultHeight);
      let oldTop = model.getTop();

      // Snap to the first line.
      snapForward(mode);
      let newTop = model.getTop();
      assertEquals(0, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(1, keyboardLines);

      // Snap to the second line.
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(2, keyboardLines);

      // Snap to the last line.
      oldTop = newTop;
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(3, keyboardLines);

      // There's only 3 text lines so moving forward should not change position.
      oldTop = newTop;
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertEquals(3, keyboardLines);

      // Snap back to the second line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(4, keyboardLines);

      // Snap back to the first line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(0, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(5, keyboardLines);

      // Moving back again should not change position.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(oldTop, newTop);
      assertEquals(0, model.getCurrentLineIndex());
      assertEquals(5, keyboardLines);
      assertEquals(0, speechLines);
    });

    test('snapToNextLine scrolls down to line if out of view', () => {
      mockLinesCounters();
      let oldTop = model.getTop();

      // Snap to the first line.
      snapForward(mode);
      let newTop = model.getTop();
      // Continue moving to the next line until scrolling occurs.
      while (oldTop < newTop) {
        assertEquals(0, scrollDiffReceived);
        oldTop = newTop;
        snapForward(mode);
        newTop = model.getTop();
      }

      assertLT(0, scrollDiffReceived);
      assertLT(0, keyboardLines);
      assertEquals(0, speechLines);
    });

    test('snapToNextLine scrolls up to line if out of view', () => {
      mockLinesCounters();
      model.setMinY(defaultHeight);
      model.setMaxY(defaultHeight);
      model.setCurrentLineIndex(2);
      let oldTop = model.getTop();

      // Snap to the first line.
      snapBackward(mode);
      let newTop = model.getTop();
      // Continue moving to the previous line until scrolling occurs.
      while (oldTop > newTop) {
        assertEquals(0, scrollDiffReceived);
        oldTop = newTop;
        snapBackward(mode);
        newTop = model.getTop();
      }

      assertGT(0, scrollDiffReceived);
      assertLT(0, keyboardLines);
      assertEquals(0, speechLines);
    });

    test('snapToNextLine after user scroll uses current position', () => {
      mockLinesCounters();

      snapForward(mode);
      mode.onScrollEnd(defaultHeight);
      snapForward(mode);

      assertEquals(2, keyboardLines);
    });

    test('snapToNextLine with window moves by line', () => {
      mode = createWindowMode();
      mockLinesCounters();
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      const rect4 = new DOMRect(0, 70, 100, 20);
      const rect5 = new DOMRect(0, 90, 100, 20);
      model.setTextBounds([rect1, rect2, rect3, rect4, rect5]);
      model.setMaxY(defaultHeight);
      let oldTop = model.getTop();

      // Snap to the second line.
      snapForward(mode);
      let newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(3, keyboardLines);

      // Snap to the third line.
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(4, keyboardLines);

      // Snap to the fourth line.
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(3, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(5, keyboardLines);

      // Moving forward should not change position.
      oldTop = newTop;
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(3, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertEquals(5, keyboardLines);

      // Snap back to the third line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(6, keyboardLines);

      // Snap back to the second line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(7, keyboardLines);

      // Moving back again should not change position since the window is 3
      // lines long and it is already surrounding the second line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertEquals(7, keyboardLines);
      assertEquals(0, speechLines);
    });

    test('snapToNextLine returns true with text bounds', () => {
      mockLinesCounters();
      model.setTextBounds([]);
      model.setMaxY(defaultHeight);

      assertFalse(mode.snapToNextLine(true));
      assertFalse(mode.snapToNextLine(false));

      setDefaultTextBounds();
      assertTrue(mode.snapToNextLine(true));
      assertTrue(mode.snapToNextLine(false));
    });
  });

  suite('none mode', () => {
    let mode: LineFocusNoneMoveMode;

    setup(() => {
      mode = new LineFocusNoneMoveMode(
          model, styleMode, delegate, LineFocusMovement.STATIC);
    });

    test('getMovement returns movement from constructor', () => {
      assertEquals(LineFocusMovement.STATIC, mode.getMovement());

      const cursorMode = new LineFocusNoneMoveMode(
          model, styleMode, delegate, LineFocusMovement.CURSOR);
      assertEquals(LineFocusMovement.CURSOR, cursorMode.getMovement());
    });

    test('onActivated ends active session and resets model', () => {
      model.setSessionActive(true);
      model.setTop(100);
      model.setWindowHeight(100);
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertTrue(sessionEnded);
      assertFalse(model.isSessionActive());
      assertEquals(0, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onActivated does not end inactive session', () => {
      model.setSessionActive(false);
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertFalse(sessionEnded);
    });

    test('onActivated does not update positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(0, model.getMaxY());
      assertEquals(0, model.getMinY());
      assertEquals(0, model.getTextBounds().length);
    });

    test('onActivated notifies delegate of no scroll buffer', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      // Need to do a direct compare since the starting value is undefined.
      assertTrue(bufferValReceived === false);
    });

    test('onWordBoundary does nothing', () => {
      const container = createShortContainer();
      NodeStore.getInstance().setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertFalse(notifiedMove);
      assertFalse(model.getInitiatedScroll());
      assertEquals(0, scrollDiffReceived);
    });

    test('onMouseMove does nothing', () => {
      mode.onMouseMove(101);
      assertFalse(notifiedMove);
    });

    test('onScrollEnd does nothing', () => {
      mode.onScrollEnd(101);
      assertFalse(notifiedMove);
    });

    test('onTextLocationsChange does nothing', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertFalse(!!bufferValReceived);
      assertFalse(notifiedMove);
      assertEquals(0, scrollDiffReceived);
      assertEquals(0, model.getMaxY());
      assertEquals(0, model.getMinY());
      assertEquals(0, model.getTextBounds().length);
    });

    test('onScrollEnd does nothing', () => {
      mode.onScrollEnd(101);
      assertFalse(notifiedMove);
    });

    test('onTextLocationsChange does nothing', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertFalse(!!bufferValReceived);
      assertFalse(notifiedMove);
      assertEquals(0, scrollDiffReceived);
      assertEquals(0, model.getMaxY());
      assertEquals(0, model.getMinY());
      assertEquals(0, model.getTextBounds().length);
    });

    test('snapToNextLine does nothing', () => {
      assertFalse(mode.snapToNextLine(true));
      assertFalse(mode.snapToNextLine(false));
      assertEquals(null, model.getCurrentLineIndex());
    });
  });
});
