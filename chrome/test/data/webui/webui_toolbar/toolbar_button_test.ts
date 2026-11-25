// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {isMac} from 'chrome://resources/js/platform.js';
import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, ContextMenuType, getClickSourceType, getContextMenuSourceType, HighlightTracker, isMacContextClick, isPrimaryClick, PressHandler} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';
import type {MediaButtonElement, OverflowButtonElement, SplitTabsButtonElement} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

suite('ToolbarButtonTest', function() {
  test('IsMacContextClick', function() {
    assertFalse(
        isMacContextClick(new PointerEvent('pointerdown', {button: 0})));
    assertFalse(isMacContextClick(
        new PointerEvent('pointerdown', {button: 1, ctrlKey: true})));
    assertFalse(isMacContextClick(
        new PointerEvent('pointerdown', {button: 2, ctrlKey: true})));
    assertEquals(
        isMac,
        isMacContextClick(
            new PointerEvent('pointerdown', {button: 0, ctrlKey: true})));
  });

  test('IsPrimaryClick', function() {
    assertTrue(isPrimaryClick(new PointerEvent('pointerdown', {button: 0})));
    assertFalse(isPrimaryClick(new PointerEvent('pointerdown', {button: 1})));
    assertFalse(isPrimaryClick(new PointerEvent('pointerdown', {button: 2})));
    assertEquals(
        !isMac,
        isPrimaryClick(
            new PointerEvent('pointerdown', {button: 0, ctrlKey: true})));
  });

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

    // Physical pointer events natively have detail == 0 -> kMouse
    for (const type of ['pointerdown', 'pointerup', 'pointermove']) {
      assertEquals(
          MenuSourceType.kMouse,
          getClickSourceType(
              new PointerEvent(type, {pointerType: 'mouse', detail: 0})));
    }

    // Non-activation MouseEvent, detail == 0 -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getClickSourceType(new MouseEvent('mousemove', {detail: 0})));

    // KeyboardEvent -> kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getClickSourceType(new KeyboardEvent('keydown', {key: 'ArrowDown'})));
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

    // PointerEvent mouse right-click (Blink leaves detail == 0) -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getContextMenuSourceType(new PointerEvent(
            'contextmenu', {pointerType: 'mouse', button: 2, detail: 0})));

    // PointerEvent Mac Ctrl+LeftClick -> kMouse
    assertEquals(
        MenuSourceType.kMouse,
        getContextMenuSourceType(new PointerEvent('contextmenu', {
          pointerType: 'mouse',
          button: 0,
          ctrlKey: true,
          detail: 0,
        })));

    // PointerEvent from keyboard (Menu key / Shift+F10), no button ->
    // kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getContextMenuSourceType(new PointerEvent(
            'contextmenu', {pointerType: 'mouse', button: -1, detail: 0})));

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

    // KeyboardEvent -> kKeyboard
    assertEquals(
        MenuSourceType.kKeyboard,
        getContextMenuSourceType(
            new KeyboardEvent('keydown', {key: 'ArrowDown'})));
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
      assertEquals(
          !isMac,
          clickSkipped(
              createHighlightedTracker(),
              new PointerEvent(
                  'pointerdown',
                  {button: 0, ctrlKey: true, pointerType: 'mouse'})));
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

  // Creates a mock HTMLElement with a predefined bounding client rect, and adds
  // it to the document, since PressHandler ignores presses on disconnected
  // elements. This is used to test click coordinates and check if they are
  // within bounds.
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
    document.body.appendChild(target);
    return target;
  }

  // Dispatches a PointerEvent (pointerdown or pointerup) targeting the mock
  // element with coordinates pointing to its center, unless `clientY` is
  // specified.
  function dispatchPointerEvent(
      handler: PressHandler, eventType: 'pointerdown'|'pointerup',
      target: HTMLElement, pointerId: number, ctrlKey: boolean = false,
      clientY: number = TARGET_MIDDLE, pointerType: string = '') {
    const event = new PointerEvent(eventType, {
      bubbles: true,
      cancelable: true,
      pointerId,
      pointerType,
      button: 0,
      ctrlKey,
      clientX: TARGET_MIDDLE,
      clientY,
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
  let canLongPress: boolean;
  let handler: PressHandler;
  let target: HTMLElement;

  setup(() => {
    shortPressCount = 0;
    longPressCount = 0;
    canLongPress = true;
    handler = new PressHandler(
        (_source) => {
          longPressCount++;
        },
        (_e) => {
          shortPressCount++;
        },
        () => canLongPress);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

  suite('LongPress', function() {
    const LONG_PRESS_MS = PressHandler.LONG_PRESS_TIMER_THRESHOLD_MS;
    const DRAG_DISTANCE_PX = PressHandler.DRAG_THRESHOLD_PX + 1;

    let mockTimer: MockTimer;

    // Mocks pointer capture on `element`. Synthetic events have no active
    // pointer to capture, so the real methods can't be used.
    function mockPointerCapture(element: HTMLElement) {
      let hasCapture = false;
      element.setPointerCapture = (_id) => {
        hasCapture = true;
      };
      element.releasePointerCapture = (_id) => {
        hasCapture = false;
      };
      element.hasPointerCapture = (_id) => hasCapture;
    }

    setup(() => {
      mockTimer = new MockTimer();
      mockTimer.install();
      mockPointerCapture(target);
    });

    teardown(() => {
      mockTimer.uninstall();
    });

    function dragDown(clientY: number = TARGET_MIDDLE + DRAG_DISTANCE_PX) {
      target.dispatchEvent(new PointerEvent('pointermove', {
        pointerId: 1,
        clientX: TARGET_MIDDLE,
        clientY,
      }));
    }

    test('Timer', function() {
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS - 1);
      assertEquals(0, longPressCount);
      mockTimer.tick(1);
      assertEquals(1, longPressCount);

      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(0, shortPressCount);
      assertEquals(1, longPressCount);
    });

    test('Drag', function() {
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      dragDown();
      assertEquals(1, longPressCount);

      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(0, shortPressCount);
      assertEquals(1, longPressCount);

      // The timer should have been stopped.
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(1, longPressCount);
    });

    // If long presses aren't allowed when the timer fires, the press should be
    // treated as a short press on release.
    test('TimerNotAllowed', function() {
      canLongPress = false;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(0, longPressCount);

      // Once the timer has fired, dragging should not trigger a long press,
      // even if long presses are now allowed.
      canLongPress = true;
      dragDown();
      assertEquals(0, longPressCount);

      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);
    });

    // If long presses aren't allowed when dragging, the press should be treated
    // as a short press on release.
    test('DragNotAllowed', function() {
      canLongPress = false;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      dragDown();
      assertEquals(0, longPressCount);

      // Once a drag has been detected, the timer should not trigger a long
      // press, even if long presses are now allowed.
      canLongPress = true;
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(0, longPressCount);

      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);
    });

    // Whether long presses are allowed should be checked when the timer fires,
    // not when the pointer is pressed.
    test('AllowedChangesDuringPress', function() {
      canLongPress = false;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      canLongPress = true;
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(1, longPressCount);
      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(0, shortPressCount);

      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      canLongPress = false;
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(1, shortPressCount);
      assertEquals(1, longPressCount);
    });

    // A press that couldn't become a long press should be fully reset on
    // release, so subsequent presses work normally.
    test('NotAllowedThenAllowed', function() {
      canLongPress = false;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);

      canLongPress = true;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(1, longPressCount);
      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(1, shortPressCount);
    });

    // A press that couldn't become a long press should do nothing if released
    // outside the target.
    test('NotAllowedReleasedOutside', function() {
      canLongPress = false;
      const outsideY = TARGET_SIZE + DRAG_DISTANCE_PX;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      dragDown(outsideY);
      dispatchPointerEvent(
          handler, 'pointerup', target, 1, /*ctrlKey=*/ false, outsideY);
      assertEquals(0, shortPressCount);
      assertEquals(0, longPressCount);
    });

    // A press that couldn't become a long press should do nothing if
    // cancelled, and subsequent presses should work normally.
    test('NotAllowedCancelled', function() {
      canLongPress = false;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS);
      handler.onPointercancel(
          new PointerEvent('pointercancel', {pointerId: 1}));
      assertEquals(0, shortPressCount);
      assertEquals(0, longPressCount);

      simulateClick();
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);
    });

    // Matching Views, touch and pen presses that are held long enough to be
    // long presses should never result in short presses, even if long presses
    // aren't allowed.
    for (const pointerType of ['touch', 'pen']) {
      test(`TimerNotAllowed_${pointerType}`, function() {
        canLongPress = false;
        dispatchPointerEvent(
            handler, 'pointerdown', target, 1, /*ctrlKey=*/ false,
            TARGET_MIDDLE, pointerType);
        mockTimer.tick(LONG_PRESS_MS);
        dispatchPointerEvent(
            handler, 'pointerup', target, 1, /*ctrlKey=*/ false, TARGET_MIDDLE,
            pointerType);
        assertEquals(0, shortPressCount);
        assertEquals(0, longPressCount);

        // Subsequent presses should work normally.
        simulateClick();
        assertEquals(1, shortPressCount);
      });

      test(`DragNotAllowed_${pointerType}`, function() {
        canLongPress = false;
        dispatchPointerEvent(
            handler, 'pointerdown', target, 1, /*ctrlKey=*/ false,
            TARGET_MIDDLE, pointerType);
        dragDown();
        dispatchPointerEvent(
            handler, 'pointerup', target, 1, /*ctrlKey=*/ false, TARGET_MIDDLE,
            pointerType);
        assertEquals(0, shortPressCount);
        assertEquals(0, longPressCount);
      });

      test(`TimerAllowed_${pointerType}`, function() {
        dispatchPointerEvent(
            handler, 'pointerdown', target, 1, /*ctrlKey=*/ false,
            TARGET_MIDDLE, pointerType);
        mockTimer.tick(LONG_PRESS_MS);
        assertEquals(1, longPressCount);
        dispatchPointerEvent(
            handler, 'pointerup', target, 1, /*ctrlKey=*/ false, TARGET_MIDDLE,
            pointerType);
        assertEquals(0, shortPressCount);
      });

      // Short touch and pen presses should still work.
      test(`ShortPress_${pointerType}`, function() {
        canLongPress = false;
        dispatchPointerEvent(
            handler, 'pointerdown', target, 1, /*ctrlKey=*/ false,
            TARGET_MIDDLE, pointerType);
        mockTimer.tick(LONG_PRESS_MS - 1);
        dispatchPointerEvent(
            handler, 'pointerup', target, 1, /*ctrlKey=*/ false, TARGET_MIDDLE,
            pointerType);
        assertEquals(1, shortPressCount);
        assertEquals(0, longPressCount);
      });
    }

    test('ContextmenuAllowed', function() {
      const contextMenuEvent = new PointerEvent('contextmenu', {
        bubbles: true,
        cancelable: true,
        button: 2,
        clientX: TARGET_MIDDLE,
        clientY: TARGET_MIDDLE,
      });
      handler.onContextmenu(contextMenuEvent);
      assertTrue(contextMenuEvent.defaultPrevented);
      assertEquals(1, longPressCount);
    });

    test('ContextmenuNotAllowed', function() {
      canLongPress = false;
      const contextMenuEvent = new PointerEvent('contextmenu', {
        bubbles: true,
        cancelable: true,
        button: 2,
        clientX: TARGET_MIDDLE,
        clientY: TARGET_MIDDLE,
      });
      handler.onContextmenu(contextMenuEvent);
      // The native context menu should still be suppressed.
      assertTrue(contextMenuEvent.defaultPrevented);
      assertEquals(0, longPressCount);
    });

    // Replaces `target` with a new element, as happens when a conditionally
    // rendered button is hidden and then shown again.
    function replaceTarget() {
      target.remove();
      target = createMockTarget();
      mockPointerCapture(target);
    }

    // If the target is removed mid-press, the press ends without a pointerup
    // or pointercancel event. The timer firing should not trigger a long
    // press, and subsequent presses should work normally.
    test('RemovedDuringPress', function() {
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      replaceTarget();
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(0, longPressCount);

      simulateClick();
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);

      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(1, shortPressCount);
      assertEquals(1, longPressCount);
    });

    // Same as above, but the target is removed after the long press timer
    // fires, when long presses aren't allowed, so the timer can't clear the
    // stale press.
    test('NotAllowedRemovedAfterTimer', function() {
      canLongPress = false;
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      mockTimer.tick(LONG_PRESS_MS);
      replaceTarget();

      simulateClick();
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);
    });

    // If the target loses pointer capture mid-press, without being removed,
    // neither dragging nor the timer should trigger a long press, and
    // releasing the pointer should not trigger a short press.
    test('CaptureLostDuringPress', function() {
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      target.releasePointerCapture(1);
      dragDown();
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(0, shortPressCount);
      assertEquals(0, longPressCount);

      simulateClick();
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);
    });

    // Presses by other pointers should be ignored while a press is active.
    test('SecondaryPointerIgnored', function() {
      dispatchPointerEvent(handler, 'pointerdown', target, 1);
      dispatchPointerEvent(handler, 'pointerdown', target, 2);
      dispatchPointerEvent(handler, 'pointerup', target, 2);
      assertEquals(0, shortPressCount);

      dispatchPointerEvent(handler, 'pointerup', target, 1);
      assertEquals(1, shortPressCount);
      assertEquals(0, longPressCount);
    });
  });
});

suite('MediaButton', function() {
  let mediaButton: MediaButtonElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mediaButton = document.createElement('media-button');
    document.body.appendChild(mediaButton);
    await microtasksFinished();
  });

  test('AriaHasPopupIsDialog', () => {
    assertTrue(!!mediaButton.shadowRoot);
    const crIconButton =
        mediaButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('dialog', crIconButton.getAttribute('aria-haspopup'));
  });

  test('RespectsReducedMotion', async () => {
    const prefersReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    const backForward = document.createElement('back-forward-button');
    const reloadButton = document.createElement('reload-button');
    const overflowButton = document.createElement('overflow-button');
    const batterySaver = document.createElement('battery-saver-button');
    const homeButton = document.createElement('home-button');
    const splitTabs = document.createElement('split-tabs-button');
    const perfButton =
        document.createElement('performance-intervention-button');
    const extraButtons = [
      backForward,
      reloadButton,
      overflowButton,
      batterySaver,
      homeButton,
      splitTabs,
      perfButton,
    ];
    const buttons = [mediaButton, ...extraButtons];
    for (const el of extraButtons) {
      document.body.appendChild(el);
    }
    try {
      await microtasksFinished();

      for (const host of buttons) {
        const crIconButton = host.shadowRoot.querySelector('cr-icon-button')!;
        assertEquals(
            prefersReducedMotion ? '0s' : '150ms ease-in-out',
            window.getComputedStyle(crIconButton)
                .getPropertyValue('--cr-icon-button-transition')
                .trim());
        const crIcon = crIconButton.shadowRoot.querySelector('cr-icon')!;
        assertEquals(
            prefersReducedMotion ? '0s, 0s' : '0.15s, 0.15s',
            window.getComputedStyle(crIcon).transitionDuration);
      }
    } finally {
      for (const el of extraButtons) {
        el.remove();
      }
    }
  });
});

suite('SplitTabsButton', function() {
  let splitTabsButton: SplitTabsButtonElement;
  let browserProxy: TestToolbarBrowserProxy;
  let browserControlsHandler: TestBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    browserControlsHandler = new TestBrowserProxy(['splitActiveTab']);
    browserProxy.browserControlsHandler = {
      splitActiveTab: () =>
          browserControlsHandler.methodCalled('splitActiveTab'),
    };
    BrowserProxyImpl.setInstance(browserProxy);

    splitTabsButton = document.createElement('split-tabs-button');
    splitTabsButton.state = {
      ...splitTabsButton.state,
      shouldBeShown: true,
    };
    document.body.appendChild(splitTabsButton);
    await microtasksFinished();
  });

  test('CtrlLeftClickUnsplit', () => {
    const button = splitTabsButton.shadowRoot.querySelector('cr-icon-button')!;
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      ctrlKey: true,
      pointerType: 'mouse',
      detail: 1,
    }));

    if (isMac) {
      assertEquals(0, browserControlsHandler.getCallCount('splitActiveTab'));
      button.dispatchEvent(new PointerEvent('contextmenu', {
        button: 0,
        ctrlKey: true,
        pointerType: 'mouse',
        detail: 1,
      }));
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
      assertEquals(
          ContextMenuType.kSplitTabsContext,
          browserProxy.toolbarUIHandler.getArgs('showContextMenu')[0][0]);
    } else {
      assertEquals(1, browserControlsHandler.getCallCount('splitActiveTab'));
    }
  });

  test('CtrlLeftClickWhileSplit', async () => {
    splitTabsButton.state = {
      ...splitTabsButton.state,
      isCurrentTabSplit: true,
      shouldBeShown: true,
    };
    await microtasksFinished();

    const button = splitTabsButton.shadowRoot.querySelector('cr-icon-button')!;
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      ctrlKey: true,
      pointerType: 'mouse',
      detail: 1,
    }));

    if (isMac) {
      assertEquals(
          0, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
      button.dispatchEvent(new PointerEvent('contextmenu', {
        button: 0,
        ctrlKey: true,
        pointerType: 'mouse',
        detail: 1,
      }));
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
      assertEquals(
          ContextMenuType.kSplitTabsContext,
          browserProxy.toolbarUIHandler.getArgs('showContextMenu')[0][0]);
    } else {
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
      assertEquals(
          ContextMenuType.kSplitTabsAction,
          browserProxy.toolbarUIHandler.getArgs('showContextMenu')[0][0]);
    }
  });
});

suite('OverflowButton', function() {
  let overflowButton: OverflowButtonElement;
  let browserProxy: TestToolbarBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    overflowButton = document.createElement('overflow-button');
    document.body.appendChild(overflowButton);
    await microtasksFinished();
  });

  test('CtrlLeftClickOnDown', () => {
    const button = overflowButton.shadowRoot.querySelector('cr-icon-button')!;
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      ctrlKey: true,
      pointerType: 'mouse',
      detail: 1,
    }));

    assertEquals(
        isMac ? 0 : 1,
        browserProxy.toolbarUIHandler.getCallCount('showOverflowMenu'));
  });
});
