// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusCursorMoveMode, LineFocusLineStyleMode, LineFocusModel, LineFocusMovement, LineFocusNoneMoveMode, LineFocusStaticMoveMode, LineFocusStyle, LineFocusWindowStyleMode, NodeStore, ReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {MoveModeDelegate} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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

  const defaultHeight = 1000;

  function createShortContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'I\'ve heard it said\nThat people come into our lives\nfor a reason.';
    container.style.whiteSpace = 'pre-line';
    document.body.appendChild(container);
    return container;
  }

  setup(() => {
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
      notifyScrollBuffer(buffer) {
        bufferValReceived = buffer;
      },
    };
  });

  suite('static mode', () => {
    let mode: LineFocusStaticMoveMode;

    setup(() => {
      // Clearing the DOM should always be done first.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      const readingMode = new FakeReadingMode();
      chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
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
      NodeStore.getInstance().setDomNode(container, 1);
      let speechLines = 0;
      chrome.readingMode.incrementLineFocusSpeechLines = () => speechLines++;
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
  });

  suite('cursor mode', () => {
    let mode: LineFocusCursorMoveMode;

    function createWindowMode(): LineFocusCursorMoveMode {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);
      return new LineFocusCursorMoveMode(model, windowMode, delegate);
    }

    setup(() => {
      mode = new LineFocusCursorMoveMode(model, styleMode, delegate);
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
      let speechLines = 0;
      chrome.readingMode.incrementLineFocusSpeechLines = () => speechLines++;
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
  });
});
