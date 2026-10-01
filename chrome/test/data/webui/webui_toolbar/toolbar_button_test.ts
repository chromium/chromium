// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac} from 'chrome://resources/js/platform.js';
import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {getClickSourceType, getContextMenuSourceType, HighlightTracker, PressHandler} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('ToolbarButtonTest', function() {
  test('GetClickSourceType', function() {
    // PointerEvent touch -> kTouch
    assertEquals(
        MenuSourceType.kTouch,
        getClickSourceType(new PointerEvent('click', {pointerType: 'touch'})));

    // PointerEvent pen -> kTouch
    assertEquals(
        MenuSourceType.kTouch,
        getClickSourceType(new PointerEvent('click', {pointerType: 'pen'})));

    // PointerEvent mouse, detail > 0 -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getClickSourceType(
            new PointerEvent('click', {pointerType: 'mouse', detail: 1})));

    // PointerEvent mouse, detail == 0 -> kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getClickSourceType(
            new PointerEvent('click', {pointerType: 'mouse', detail: 0})));

    // MouseEvent, detail == 0 -> kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getClickSourceType(new MouseEvent('click', {detail: 0})));

    // MouseEvent, detail > 0 -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getClickSourceType(new MouseEvent('click', {detail: 1})));
  });

  test('GetContextMenuSourceType', function() {
    // PointerEvent touch -> kTouch
    assertEquals(
        MenuSourceType.kTouch,
        getContextMenuSourceType(
            new PointerEvent('contextmenu', {pointerType: 'touch'})));

    // PointerEvent pen -> kTouch
    assertEquals(
        MenuSourceType.kTouch,
        getContextMenuSourceType(
            new PointerEvent('contextmenu', {pointerType: 'pen'})));

    // PointerEvent mouse, detail > 0 -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getContextMenuSourceType(new PointerEvent(
            'contextmenu', {pointerType: 'mouse', detail: 1})));

    // PointerEvent mouse, detail == 0 -> kKeyboard (Crucial test for fix)
    assertEquals(
        MenuSourceType.kKeyboard,
        getContextMenuSourceType(new PointerEvent(
            'contextmenu', {pointerType: 'mouse', detail: 0})));

    // MouseEvent left button, detail == 0 -> kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getContextMenuSourceType(
            new MouseEvent('contextmenu', {button: 0, detail: 0})));

    // MouseEvent left button, detail > 0, not ctrl -> kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getContextMenuSourceType(
            new MouseEvent('contextmenu', {button: 0, detail: 1})));

    // MouseEvent left button, detail > 0, ctrl -> kMouse (Mac Ctrl+Click)
    assertEquals(
        MenuSourceType.kMouse,
        getContextMenuSourceType(new MouseEvent(
            'contextmenu', {button: 0, detail: 1, ctrlKey: true})));

    // MouseEvent right button -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getContextMenuSourceType(new MouseEvent('contextmenu', {button: 2})));
  });

  suite('HighlightTracker', function() {
    const leftMouseDown =
        new PointerEvent('pointerdown', {button: 0, pointerType: 'mouse'});
    const mouseClick = new PointerEvent('click', {pointerType: 'mouse'});

    // Returns a tracker whose bubble is open.
    function createHighlightedTracker(): HighlightTracker {
      const tracker = new HighlightTracker();
      tracker.onHighlightChanged(true);
      return tracker;
    }

    // Simulates a click whose pointerdown is `down`, and returns whether the
    // click was skipped.
    function clickSkipped(
        tracker: HighlightTracker, down: PointerEvent,
        click: PointerEvent = mouseClick): boolean {
      tracker.onPointerdown(down);
      return tracker.shouldSkipClick(click);
    }

    test('NonPrimaryButtonNotSkipped', function() {
      assertFalse(clickSkipped(
          createHighlightedTracker(),
          new PointerEvent('pointerdown', {button: 1, pointerType: 'mouse'})));
      assertFalse(clickSkipped(
          createHighlightedTracker(),
          new PointerEvent('pointerdown', {button: 2, pointerType: 'mouse'})));
    });

    test('KeyboardNotSkipped', function() {
      // Empty pointerType on pointerdown (e.g. keyboard).
      assertFalse(clickSkipped(
          createHighlightedTracker(),
          new PointerEvent('pointerdown', {button: 0, pointerType: ''})));
      // Keyboard click, even after a pointerdown that dismissed the bubble.
      assertFalse(clickSkipped(
          createHighlightedTracker(), leftMouseDown,
          new PointerEvent('click', {pointerType: ''})));
    });

    test('SkippedWhenBubbleOpen', function() {
      const tracker = createHighlightedTracker();
      assertTrue(clickSkipped(tracker, leftMouseDown));
      // Only the one click following the pointerdown is skipped.
      assertFalse(tracker.shouldSkipClick(mouseClick));
    });

    test('NotSkippedWhenBubbleClosed', function() {
      // TODO(crbug.com/568351167): Fails on Mac.
      if (isMac) {
        this.skip();
      }

      assertFalse(clickSkipped(new HighlightTracker(), leftMouseDown));

      // Bubble closed more than 100ms ago.
      const tracker = new HighlightTracker();
      tracker.lastUnhighlightedTime = performance.now() - 200;
      assertFalse(clickSkipped(tracker, leftMouseDown));
    });

    test('SkippedWhenBubbleRecentlyClosed', function() {
      // The highlight may be removed before the pointerdown that dismissed the
      // bubble arrives.
      const tracker = createHighlightedTracker();
      tracker.onHighlightChanged(false);
      assertTrue(clickSkipped(tracker, leftMouseDown));
    });
  });

  const TARGET_SIZE = 100;
  const TARGET_MIDDLE = TARGET_SIZE / 2;

  // Creates a mock HTMLElement with a predefined bounding client rect.
  // This is used to test click coordinates and check if they are within bounds.
  function createMockTarget(): HTMLElement {
    const target = document.createElement('div');
    target.getBoundingClientRect = () => {
      return {
        x: 0,
        y: 0,
        width: TARGET_SIZE,
        height: TARGET_SIZE,
        top: 0,
        right: TARGET_SIZE,
        bottom: TARGET_SIZE,
        left: 0,
        toJSON: () => {},
      };
    };
    return target;
  }

  // Dispatches a PointerEvent (pointerdown or pointerup) targeting the mock
  // element with coordinates pointing to its center.
  function dispatchPointerEvent(
      handler: PressHandler, eventType: 'pointerdown'|'pointerup',
      target: HTMLElement, pointerId: number, ctrlKey: boolean = false) {
    const event = new PointerEvent(eventType, {
      bubbles: true,
      cancelable: true,
      pointerId,
      button: 0,
      ctrlKey,
      clientX: TARGET_MIDDLE,
      clientY: TARGET_MIDDLE,
    });
    Object.defineProperty(
        event, 'currentTarget', {value: target, writable: false});
    if (eventType === 'pointerdown') {
      handler.onPointerdown(event);
    } else {
      handler.onPointerup(event);
    }
  }

  let shortPressCount: number;
  let longPressCount: number;
  let handler: PressHandler;
  let target: HTMLElement;

  setup(() => {
    shortPressCount = 0;
    longPressCount = 0;
    handler = new PressHandler(
        (_source) => {
          longPressCount++;
        },
        (_e) => {
          shortPressCount++;
        });
    target = createMockTarget();
  });

  function simulateClick(pointerId: number = 1) {
    dispatchPointerEvent(handler, 'pointerdown', target, pointerId);
    dispatchPointerEvent(handler, 'pointerup', target, pointerId);
  }

  // Tests a standard mouse click where pointer capture is successful.
  // The click should successfully trigger the short press callback.
  test('PressHandlerNormalClick', function() {
    let hasCapture = false;
    target.setPointerCapture = (_id) => {
      hasCapture = true;
    };
    target.hasPointerCapture = (_id) => hasCapture;

    simulateClick();

    assertEquals(1, shortPressCount);
    assertEquals(0, longPressCount);
  });

  // Tests synthetic mouse clicks from screen readers where pointer capture
  // fails (e.g. because there's no physical pointer to capture).
  // The press handler should detect the capture failure on pointerdown
  // and still trigger the short press on pointerup.
  test('PressHandlerScreenReaderSyntheticClick', function() {
    target.setPointerCapture = (_id) => {};
    target.hasPointerCapture = (_id) => false;

    simulateClick();

    assertEquals(1, shortPressCount);
    assertEquals(0, longPressCount);
  });

  // Tests the case where pointer capture is successfully acquired but later
  // lost before the pointerup event occurs (e.g. by another element capturing).
  // In this case, the short press callback should NOT trigger.
  test('PressHandlerClickWithoutCapture', function() {
    let hasCapture = false;
    target.setPointerCapture = (_id) => {
      hasCapture = true;
    };
    target.hasPointerCapture = (_id) => hasCapture;

    dispatchPointerEvent(handler, 'pointerdown', target, 1);
    hasCapture = false;  // Lose capture
    dispatchPointerEvent(handler, 'pointerup', target, 1);

    assertEquals(0, shortPressCount);
    assertEquals(0, longPressCount);
  });

  // Tests that on Mac, a Ctrl+LeftClick bypasses pointer capture and standard
  // short press logic, delegating to the native contextmenu event instead.
  test('PressHandlerMacCtrlLeftClick', function() {
    if (!isMac) {
      return;
    }

    let hasCapture = false;
    target.setPointerCapture = (_id) => {
      hasCapture = true;
    };
    target.hasPointerCapture = (_id) => hasCapture;

    dispatchPointerEvent(handler, 'pointerdown', target, 1, /*ctrlKey=*/ true);

    // On Mac, pointer capture should be bypassed.
    assertFalse(hasCapture);

    // Simulate the native contextmenu event that fires immediately after.
    const contextMenuEvent = new PointerEvent('contextmenu', {
      bubbles: true,
      cancelable: true,
      button: 0,
      ctrlKey: true,
      clientX: TARGET_MIDDLE,
      clientY: TARGET_MIDDLE,
    });
    Object.defineProperty(
        contextMenuEvent, 'currentTarget', {value: target, writable: false});
    handler.onContextmenu(contextMenuEvent);

    // Long press should be triggered by contextmenu.
    assertEquals(0, shortPressCount);
    assertEquals(1, longPressCount);

    dispatchPointerEvent(handler, 'pointerup', target, 1, /*ctrlKey=*/ true);

    // Short press should NOT be triggered.
    assertEquals(0, shortPressCount);
    assertEquals(1, longPressCount);
  });

  // Tests that on non-Mac platforms, a Ctrl+LeftClick is treated as a normal
  // click that captures the pointer and triggers a short press (e.g., Hard
  // Reload for reload button).
  test('PressHandlerNonMacCtrlLeftClick', function() {
    if (isMac) {
      return;
    }

    let hasCapture = false;
    target.setPointerCapture = (_id) => {
      hasCapture = true;
    };
    target.hasPointerCapture = (_id) => hasCapture;

    dispatchPointerEvent(handler, 'pointerdown', target, 1, /*ctrlKey=*/ true);

    // On non-Mac, it should be captured normally.
    assertTrue(hasCapture);

    dispatchPointerEvent(handler, 'pointerup', target, 1, /*ctrlKey=*/ true);

    // Short press should be triggered.
    assertEquals(1, shortPressCount);
    assertEquals(0, longPressCount);
  });
});
