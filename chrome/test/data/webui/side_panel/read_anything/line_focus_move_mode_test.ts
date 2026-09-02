// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LineFocusCursorMoveMode, LineFocusLineStyleMode, LineFocusModel, LineFocusMovement, LineFocusNoneMoveMode, LineFocusStaticMoveMode, LineFocusStyle, LineFocusWindowStyleMode, ReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineFocusMoveMode, MoveModeDelegate, NodeStore, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setupTestEnvironment} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('LineFocusMoveMode', () => {
  let model: LineFocusModel;
  let styleMode: LineFocusLineStyleMode;
  let windowMode: LineFocusWindowStyleMode;
  let delegate: MoveModeDelegate;
  let notifiedContentPositionChange: boolean;
  let notifiedVisualPositionChange: boolean;
  let scrollDiffReceived: number;
  let instantScrollReceived: boolean|undefined;
  let bufferValReceived: boolean|undefined;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;
  let speechController: SpeechController;
  let nodeStore: NodeStore;

  const defaultHeight = 1000;

  function getKeyboardLines(): number {
    return metricsBrowserProxy.getCallCount('incrementLineFocusKeyboardLines');
  }

  function getSpeechLines(): number {
    return metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines');
  }

  function createShortContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'I\'ve heard it said\nThat people come into our lives\nfor a reason.\n';
    container.style.whiteSpace = 'pre';
    container.style.width = 'max-content';
    container.style.margin = '0';
    container.style.fontSize = '20px';
    container.style.lineHeight = '2';
    document.body.appendChild(container);
    return container;
  }

  // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to be
  // gone.
  function getVisualLines(container: HTMLElement): string[] {
    const walker = document.createTreeWalker(
        container,
        NodeFilter.SHOW_TEXT,
        null,
    );
    const range = document.createRange();
    const lines: string[] = [];
    let currentLine = '';
    let lastTop: number|null = null;

    let node = walker.nextNode();
    while (node) {
      const text = node.textContent || '';
      for (let i = 0; i < text.length; i++) {
        range.setStart(node, i);
        range.setEnd(node, i + 1);
        const rect = range.getBoundingClientRect();
        if (rect.width === 0 && rect.height === 0) {
          continue;
        }
        if (lastTop === null) {
          lastTop = rect.top;
        } else if (Math.abs(rect.top - lastTop) > 2) {
          lines.push(currentLine);
          currentLine = '';
          lastTop = rect.top;
        }
        currentLine += text[i];
      }
      node = walker.nextNode();
    }
    if (currentLine) {
      lines.push(currentLine);
    }
    return lines;
  }

  // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to be
  // gone.
  function logBoundsFailure(testName: string, container: HTMLElement) {
    const rect = container.getBoundingClientRect();
    const bounds = model.getTextBounds();
    const visualLines = getVisualLines(container);
    console.error(
        `[${testName}] text bounds length is ${bounds.length}, expected 3.\n` +
        `Container: width=${container.offsetWidth}px, height=${
            container.offsetHeight}px, ` +
        `rect=[left:${rect.left}, top:${rect.top}, width:${
            rect.width}, height:${rect.height}], ` +
        `computedStyle.whiteSpace="${
            window.getComputedStyle(container).whiteSpace}"\n` +
        `Window: innerWidth=${window.innerWidth}px, body.clientWidth=${
            document.body.clientWidth}px\n` +
        `Visual lines (${visualLines.length}):\n` +
        visualLines.map((line, i) => `  [${i}]: "${line}"`).join('\n') + '\n' +
        `HTML: ${container.outerHTML}\n` +
        `Bounds: ${JSON.stringify(bounds.map(b => ({
                                               top: b.top,
                                               bottom: b.bottom,
                                               left: b.left,
                                               right: b.right,
                                             })))}`);
  }

  function mockLinesCounters() {
    metricsBrowserProxy.reset();
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
    const result = setupTestEnvironment();
    nodeStore = result.nodeStore;
    speechController = result.speechController;
    metricsBrowserProxy = result.metrics;
    visualBrowserProxy = result.visualBrowserProxy;
    // Initialize font size so that the threshold for merging text bounds
    // is correctly calculated and not zero.
    visualBrowserProxy.fontSize = 1.5;
    model = new LineFocusModel();
    styleMode = new LineFocusLineStyleMode(LineFocusStyle.UNDERLINE, model);
    windowMode =
        new LineFocusWindowStyleMode(LineFocusStyle.MEDIUM_WINDOW, model);
    instantScrollReceived = undefined;
    notifiedContentPositionChange = false;
    notifiedVisualPositionChange = false;
    scrollDiffReceived = 0;
    bufferValReceived = undefined;
    delegate = {
      notifyMoveWithContentPositionChange() {
        notifiedContentPositionChange = true;
      },
      notifyMoveWithVisualPositionChange() {
        notifiedVisualPositionChange = true;
      },
      notifyScroll(diff, instant) {
        scrollDiffReceived = diff;
        instantScrollReceived = instant;
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
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(
          1, metricsBrowserProxy.getCallCount('startLineFocusSession'));
      assertTrue(model.isSessionActive());
    });

    test('onActivated should not adapt multi-line window', () => {
      const container = document.createElement('div');
      mode.onActivated(container, defaultHeight);
      assertFalse(model.getAdaptMultiLineWindow());
    });

    test('onActivated updates positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      if (model.getTextBounds().length !== 3) {
        logBoundsFailure('static onActivated', container);
      }
      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onActivated scrolls to first line', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      assertNotEquals(0, scrollDiffReceived);
    });

    test('onActivated sets center focal point', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      const expectedFocalPoint = styleMode.getCenterY();
      assertEquals(expectedFocalPoint, model.getFocalPoint());
      assertTrue(notifiedContentPositionChange);
    });

    test('onActivated notifies delegate of scroll buffer', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      // Need to do a direct compare since the starting value is undefined.
      assertTrue(bufferValReceived === true);
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertEquals(
          0, metricsBrowserProxy.getCallCount('startLineFocusSession'));
      assertTrue(model.isSessionActive());
    });

    test('onWordBoundary scrolls to line', () => {
      const container = createShortContainer();
      model.setMaxY(defaultHeight * 2);
      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 7,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertFalse(notifiedContentPositionChange);
      assertTrue(model.getInitiatedScroll());
    });

    test('onWordBoundary scrolls to line if it would go off screen', () => {
      const container = createShortContainer();
      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 7,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertFalse(notifiedContentPositionChange);
      assertTrue(model.getInitiatedScroll());
    });

    test('onWordBoundary only counts new lines', () => {
      const container = createShortContainer();
      mockLinesCounters();
      nodeStore.setDomNode(container, 1);
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
      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      const callCount1 =
          metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines');
      if (callCount1 !== 1) {
        console.error(`static onWordBoundary segment1 speech lines is ${
            callCount1}, expected 1. Focal point: ${
            model.getFocalPoint()}, scrollDiff: ${scrollDiffReceived}`);
      }
      assertEquals(
          1, metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines'));

      // Mock the panel scroll so the next segment is on the same line.
      model.setFocalPoint(model.getFocalPoint() + scrollDiffReceived);
      mode.onWordBoundary(segments2);
      assertLT(0, scrollDiffReceived);
      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      const callCount2 =
          metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines');
      if (callCount2 !== 1) {
        console.error(`static onWordBoundary segment2 speech lines is ${
            callCount2}, expected 1. Focal point: ${
            model.getFocalPoint()}, scrollDiff: ${scrollDiffReceived}`);
      }
      assertEquals(
          1, metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines'));
    });

    test('onMouseMove does nothing', () => {
      mode.onMouseMove(101);
      assertFalse(notifiedContentPositionChange);
    });

    test('onMouseMoveInToolbar does nothing', () => {
      mode.onMouseMoveInToolbar(101);
      assertFalse(notifiedContentPositionChange);
    });

    test('onScrollEnd adds scroll distance', () => {
      const top1 = 43;
      const top2 = 55;
      const top3 = 12;
      // Ensure we test scrolling up and down;
      assertLT(top1, top2);
      assertGT(top2, top3);

      mode.onScrollEnd(top1);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          1, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
      assertEquals(
          top1, metricsBrowserProxy.getArgs('addLineFocusScrollDistance')[0]);

      mode.onScrollEnd(top2);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          2, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
      assertEquals(
          top2 - top1,
          metricsBrowserProxy.getArgs('addLineFocusScrollDistance')[1]);

      mode.onScrollEnd(top3);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          3, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
      assertEquals(
          top2 - top3,
          metricsBrowserProxy.getArgs('addLineFocusScrollDistance')[2]);
    });

    test('onScrollEnd notifies content position change for user scroll', () => {
      model.setInitiatedScroll(false);
      mode.onScrollEnd(100);

      assertTrue(notifiedContentPositionChange);
    });

    test(
        'onScrollEnd notifies content position change for single-line window only on line focus scroll',
        () => {
          const rect1 = new DOMRect(0, 10, 100, 20);
          const rect2 = new DOMRect(0, 30, 100, 20);
          const rect3 = new DOMRect(0, 50, 100, 20);
          model.setTextBounds([rect1, rect2, rect3]);
          model.setCurrentLineIndex(1);
          model.setTop(10);
          model.setWindowHeight(10);

          // Underline style does nothing.
          model.setInitiatedScroll(true);
          mode.onScrollEnd(100);
          assertFalse(notifiedContentPositionChange);

          // Medium window does nothing.
          model.setInitiatedScroll(true);
          mode = new LineFocusStaticMoveMode(model, windowMode, delegate);
          mode.onScrollEnd(100);
          assertFalse(notifiedContentPositionChange);

          // Small window notifies of move.
          model.setInitiatedScroll(true);
          const singleWindow =
              new LineFocusWindowStyleMode(LineFocusStyle.SMALL_WINDOW, model);
          mode = new LineFocusStaticMoveMode(model, singleWindow, delegate);
          mode.onScrollEnd(100);
          assertTrue(notifiedContentPositionChange);
        });

    test('onTextLocationsChange updates scroll buffer', () => {
      const container = createShortContainer();
      mode.onTextLocationsChange(container, defaultHeight);
      assertEquals(true, bufferValReceived);
    });

    test('onTextLocationsChange updates positions', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      if (model.getTextBounds().length !== 3) {
        logBoundsFailure('static onTextLocationsChange', container);
      }
      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onTextLocationsChange re-centers focal point', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertTrue(notifiedVisualPositionChange);
      const center = styleMode.getCenterY();
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
      assertEquals(1, getKeyboardLines());

      // Snap to the second line.
      snapForward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(1, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertLT(oldScrollDiff, newScrollDiff);
      assertEquals(2, getKeyboardLines());

      // Snap to the last line.
      oldTop = newTop;
      oldScrollDiff = newScrollDiff;
      snapForward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(2, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertLT(oldScrollDiff, newScrollDiff);
      assertEquals(3, getKeyboardLines());

      // Snap back to the second line.
      oldTop = newTop;
      oldScrollDiff = newScrollDiff;
      snapBackward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(1, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertGT(oldScrollDiff, newScrollDiff);
      assertEquals(4, getKeyboardLines());

      // Snap back to the first line.
      oldTop = newTop;
      oldScrollDiff = newScrollDiff;
      snapBackward(mode);
      newTop = model.getTop();
      newScrollDiff = scrollDiffReceived;
      assertEquals(0, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertGT(oldScrollDiff, newScrollDiff);
      assertEquals(5, getKeyboardLines());
      assertEquals(0, getSpeechLines());
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
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertEquals(
          1, metricsBrowserProxy.getCallCount('startLineFocusSession'));
      assertTrue(model.isSessionActive());
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

      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      if (model.getTextBounds().length !== 3) {
        logBoundsFailure('cursor onActivated', container);
      }
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
      assertTrue(notifiedContentPositionChange);
    });

    test('onActivated from on does not move to first line', () => {
      const container = createShortContainer();
      model.setSessionActive(true);
      const focalPoint = 100;
      model.setFocalPoint(focalPoint);

      mode.onActivated(container, defaultHeight);

      assertEquals(focalPoint, model.getFocalPoint());
      assertEquals(null, model.getCurrentLineIndex());
      assertTrue(notifiedVisualPositionChange);
    });

    test('onActivated notifies delegate of no scroll buffer', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      // Need to do a direct compare since the starting value is undefined.
      assertTrue(bufferValReceived === false);
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertEquals(
          0, metricsBrowserProxy.getCallCount('startLineFocusSession'));
      assertTrue(model.isSessionActive());
    });

    test('onWordBoundary updates position', () => {
      const container = createShortContainer();
      model.setMaxY(defaultHeight * 2);
      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertEquals(0, scrollDiffReceived);
      assertTrue(notifiedContentPositionChange);
    });

    test('onWordBoundary scrolls if line would go off screen', () => {
      const container = createShortContainer();
      model.setMaxY(10);
      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertTrue(notifiedContentPositionChange);
      assertTrue(model.getInitiatedScroll());
    });

    test('onWordBoundary only counts new lines', () => {
      const container = createShortContainer();
      mockLinesCounters();
      nodeStore.setDomNode(container, 1);
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
      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      const callCount1 =
          metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines');
      if (callCount1 !== 1) {
        console.error(`cursor onWordBoundary segment1 speech lines is ${
            callCount1}, expected 1. Focal point: ${model.getFocalPoint()}`);
      }
      assertEquals(
          1, metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines'));

      mode.onWordBoundary(segments2);
      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      const callCount2 =
          metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines');
      if (callCount2 !== 1) {
        console.error(`cursor onWordBoundary segment2 speech lines is ${
            callCount2}, expected 1. Focal point: ${model.getFocalPoint()}`);
      }
      assertEquals(
          1, metricsBrowserProxy.getCallCount('incrementLineFocusSpeechLines'));
    });

    test('onWordBoundary initializes bounds if empty', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      model.setTextBounds([]);
      assertEquals(0, model.getTextBounds().length);

      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);
      assertGT(model.getTextBounds().length, 0);
    });

    test(
        'onWordBoundary does not recompute bounds if already populated and scroll unchanged',
        () => {
          const container = createShortContainer();
          mode.onActivated(container, defaultHeight);
          const customBounds = [new DOMRect(0, 50, 200, 20)];
          model.setTextBounds(customBounds);

          nodeStore.setDomNode(container, 1);
          const segments = [{
            node: ReadAloudNode.create(container)!,
            start: 0,
            length: 5,
          }];

          mode.onWordBoundary(segments);
          assertEquals(50, model.getTextBounds()[0]!.top);
        });

    test('onWordBoundary scrolls instantly during active speech', () => {
      const container = createShortContainer();
      mode.onActivated(container, defaultHeight);
      model.setMaxY(10);

      speechController.isSpeechActive = () => true;

      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertLT(0, scrollDiffReceived);
      assertTrue(instantScrollReceived === true);
      assertTrue(notifiedContentPositionChange);
    });

    test(
        'onWordBoundary refreshes bounds if scroller scrollTop changed', () => {
          const scroller = document.createElement('div');
          scroller.className = 'sp-scroller';
          Object.defineProperty(scroller, 'scrollTop', {
            value: 0,
            writable: true,
          });
          const container = createShortContainer();
          scroller.appendChild(container);
          document.body.appendChild(scroller);

          mode.onActivated(container, defaultHeight);
          const customBounds = [new DOMRect(0, 50, 200, 20)];
          model.setTextBounds(customBounds);
          model.setLastScrollTop(0);

          // Simulate a scroll occurred prior to onWordBoundary.
          scroller.scrollTop = 150;

          nodeStore.setDomNode(container, 1);
          const segments = [{
            node: ReadAloudNode.create(container)!,
            start: 0,
            length: 5,
          }];

          mode.onWordBoundary(segments);

          // Last scroll top should be updated to match the new
          // scroller.scrollTop.
          assertEquals(150, model.getLastScrollTop());
        });

    test('onMouseMove adds mouse distance', () => {
      const y1 = 43;
      const y2 = 55;
      const y3 = 32;
      // Ensure we test moving up and down;
      assertLT(y1, y2);
      assertGT(y2, y3);

      mode.onMouseMove(y1);
      assertEquals(
          1, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          y1, metricsBrowserProxy.getArgs('addLineFocusMouseDistance')[0]);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));

      mode.onMouseMove(y2);
      assertEquals(
          2, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          y2 - y1, metricsBrowserProxy.getArgs('addLineFocusMouseDistance')[1]);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));

      mode.onMouseMove(y3);
      assertEquals(
          3, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          y2 - y3, metricsBrowserProxy.getArgs('addLineFocusMouseDistance')[2]);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
    });

    test('onMouseMove notifies listeners', () => {
      mode.onMouseMove(101);
      assertTrue(notifiedContentPositionChange);
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
      assertFalse(notifiedContentPositionChange);
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

      // The line should align with the bottom of the first visible line.
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
      const top1 = 43;
      const top2 = 55;
      const top3 = 12;
      // Ensure we test scrolling up and down;
      assertLT(top1, top2);
      assertGT(top2, top3);

      mode.onScrollEnd(top1);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          1, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
      assertEquals(
          top1, metricsBrowserProxy.getArgs('addLineFocusScrollDistance')[0]);

      mode.onScrollEnd(top2);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          2, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
      assertEquals(
          top2 - top1,
          metricsBrowserProxy.getArgs('addLineFocusScrollDistance')[1]);

      mode.onScrollEnd(top3);
      assertEquals(
          0, metricsBrowserProxy.getCallCount('addLineFocusMouseDistance'));
      assertEquals(
          3, metricsBrowserProxy.getCallCount('addLineFocusScrollDistance'));
      assertEquals(
          top2 - top3,
          metricsBrowserProxy.getArgs('addLineFocusScrollDistance')[2]);
    });

    test('onTextLocationsChange scrolls to re-center line focus', () => {
      const container = createShortContainer();
      model.setCurrentLineIndex(0);
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

      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      if (model.getTextBounds().length !== 3) {
        logBoundsFailure('cursor onTextLocationsChange', container);
      }
      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test(
        'onTextLocationsChange initializes focal point when current line index is null',
        () => {
          const container = createShortContainer();
          model.setCurrentLineIndex(null);
          model.setFocalPoint(0);

          mode.onTextLocationsChange(container, defaultHeight);

          assertLT(0, model.getFocalPoint());
          assertEquals(null, model.getCurrentLineIndex());
          assertTrue(notifiedVisualPositionChange);
        });

    test(
        'onTextLocationsChange preserves existing focal point when current line index is null',
        () => {
          const container = createShortContainer();
          const existingFocalPoint = 150;
          model.setCurrentLineIndex(null);
          model.setFocalPoint(existingFocalPoint);

          mode.onTextLocationsChange(container, defaultHeight);

          assertEquals(existingFocalPoint, model.getFocalPoint());
          assertEquals(null, model.getCurrentLineIndex());
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
      assertTrue(notifiedVisualPositionChange);
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
      assertTrue(notifiedVisualPositionChange);
    });


    test(
        'onTextLocationsChange shifts focalPoint by scrollDiff during smooth scroll',
        () => {
          // Create a scroller wrapper for the container.
          const scroller = document.createElement('div');
          scroller.className = 'sp-scroller';
          Object.defineProperty(scroller, 'scrollTop', {
            value: 0,
            writable: true,
          });
          const container = createShortContainer();
          scroller.appendChild(container);

          // Setup initial state: tracking the cursor, so current line index is
          // null.
          model.setCurrentLineIndex(null);
          model.setFocalPoint(50);
          model.setLastScrollTop(100);
          model.setInitiatedScroll(true);

          // Scroller is at initial scroll position.
          scroller.scrollTop = 100;
          mode.onTextLocationsChange(container, defaultHeight);
          notifiedVisualPositionChange = false;

          // Simulate next frame of scroll animation: scroller moved down 15px.
          // So text physically moved up 15px on screen.
          scroller.scrollTop = 115;
          mode.onTextLocationsChange(container, defaultHeight);

          // focalPoint should shift up by 15px (-15px) to track the text.
          assertEquals(50 - 15, model.getFocalPoint());
          assertTrue(notifiedVisualPositionChange);
        });

    test(
        'onTextLocationsChange shifts focalPoint and notifies during speech scroll',
        () => {
          const scroller = document.createElement('div');
          scroller.className = 'sp-scroller';
          Object.defineProperty(scroller, 'scrollTop', {
            value: 0,
            writable: true,
          });
          const container = createShortContainer();
          scroller.appendChild(container);

          speechController.isSpeechActive = () => true;

          model.setCurrentLineIndex(null);
          model.setFocalPoint(50);
          model.setLastScrollTop(100);
          model.setInitiatedScroll(false);

          scroller.scrollTop = 100;
          mode.onTextLocationsChange(container, defaultHeight);
          notifiedVisualPositionChange = false;

          scroller.scrollTop = 115;
          mode.onTextLocationsChange(container, defaultHeight);

          assertEquals(50 - 15, model.getFocalPoint());
          assertTrue(notifiedVisualPositionChange);
        });

    test(
        'onTextLocationsChange does not shift focalPoint during manual mouse scroll without speech',
        () => {
          const scroller = document.createElement('div');
          scroller.className = 'sp-scroller';
          Object.defineProperty(scroller, 'scrollTop', {
            value: 0,
            writable: true,
          });
          const container = createShortContainer();
          scroller.appendChild(container);

          speechController.isSpeechActive = () => false;

          model.setCurrentLineIndex(null);
          model.setFocalPoint(50);
          model.setLastScrollTop(100);
          model.setInitiatedScroll(false);  // User scrolled manually.

          scroller.scrollTop = 100;
          mode.onTextLocationsChange(container, defaultHeight);

          scroller.scrollTop = 115;
          mode.onTextLocationsChange(container, defaultHeight);

          // focalPoint should not shift.
          assertEquals(50, model.getFocalPoint());
        });

    test(
        'onScrollEnd notifies visual position change if speech is active',
        () => {
          speechController.isSpeechActive = () => true;
          model.setInitiatedScroll(false);

          mode.onScrollEnd(100);

          assertTrue(notifiedVisualPositionChange);
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
      assertEquals(1, getKeyboardLines());

      // Snap to the second line.
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(2, getKeyboardLines());

      // Snap to the last line.
      oldTop = newTop;
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(3, getKeyboardLines());

      // There's only 3 text lines so moving forward should not change position.
      oldTop = newTop;
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertEquals(3, getKeyboardLines());

      // Snap back to the second line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(4, getKeyboardLines());

      // Snap back to the first line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(0, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(5, getKeyboardLines());

      // Moving back again should not change position.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(oldTop, newTop);
      assertEquals(0, model.getCurrentLineIndex());
      assertEquals(5, getKeyboardLines());
      assertEquals(0, getSpeechLines());
    });

    test('snapToNextLine scrolls down to line if out of view', () => {
      mockLinesCounters();
      model.setMaxY(100);
      model.setTextBounds([
        new DOMRect(0, 0, 10, 25),
        new DOMRect(0, 30, 10, 25),
        new DOMRect(0, 60, 10, 25),
        new DOMRect(0, 90, 10, 25),
      ]);

      // The first three lines are in view so no scrolling.
      snapForward(mode);
      assertEquals(0, scrollDiffReceived);

      snapForward(mode);
      assertEquals(0, scrollDiffReceived);

      snapForward(mode);
      assertEquals(0, scrollDiffReceived);

      // The fourth line is partially out of view so scroll to center it.
      snapForward(mode);
      assertLT(0, scrollDiffReceived);
      assertEquals(4, getKeyboardLines());
      assertEquals(0, getSpeechLines());
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
      assertLT(0, getKeyboardLines());
      assertEquals(0, getSpeechLines());
    });

    test('snapToNextLine after user scroll uses current position', () => {
      mockLinesCounters();

      snapForward(mode);
      mode.onScrollEnd(defaultHeight);
      snapForward(mode);

      assertEquals(2, getKeyboardLines());
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
      assertEquals(3, getKeyboardLines());

      // Snap to the third line.
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(4, getKeyboardLines());

      // Snap to the fourth line.
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(3, model.getCurrentLineIndex());
      assertLT(oldTop, newTop);
      assertEquals(5, getKeyboardLines());

      // Moving forward should not change position.
      oldTop = newTop;
      snapForward(mode);
      newTop = model.getTop();
      assertEquals(3, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertEquals(5, getKeyboardLines());

      // Snap back to the third line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(2, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(6, getKeyboardLines());

      // Snap back to the second line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertGT(oldTop, newTop);
      assertEquals(7, getKeyboardLines());

      // Moving back again should not change position since the window is 3
      // lines long and it is already surrounding the second line.
      oldTop = newTop;
      snapBackward(mode);
      newTop = model.getTop();
      assertEquals(1, model.getCurrentLineIndex());
      assertEquals(oldTop, newTop);
      assertEquals(7, getKeyboardLines());
      assertEquals(0, getSpeechLines());
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

    test('onActivated resets model', () => {
      model.setSessionActive(true);
      model.setTop(100);
      model.setWindowHeight(100);
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertFalse(model.isSessionActive());
      assertEquals(0, model.getTop());
      assertEquals(0, model.getWindowHeight());
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
      nodeStore.setDomNode(container, 1);
      const segments = [{
        node: ReadAloudNode.create(container)!,
        start: 0,
        length: 5,
      }];

      mode.onWordBoundary(segments);

      assertFalse(notifiedContentPositionChange);
      assertFalse(model.getInitiatedScroll());
      assertEquals(0, scrollDiffReceived);
    });

    test('onMouseMove does nothing', () => {
      mode.onMouseMove(101);
      assertFalse(notifiedContentPositionChange);
    });

    test('onScrollEnd does nothing', () => {
      mode.onScrollEnd(101);
      assertFalse(notifiedContentPositionChange);
    });

    test('onTextLocationsChange does nothing', () => {
      const container = createShortContainer();

      mode.onTextLocationsChange(container, defaultHeight);

      assertFalse(!!bufferValReceived);
      assertFalse(notifiedVisualPositionChange);
      assertFalse(notifiedContentPositionChange);
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
