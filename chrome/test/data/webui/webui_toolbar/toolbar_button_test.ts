// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {getClickSourceType, getContextMenuSourceType, PressHandler} from 'chrome://webui-toolbar.top-chrome/app.js';

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
      target: HTMLElement, pointerId: number) {
    const event = new PointerEvent(eventType, {
      bubbles: true,
      cancelable: true,
      pointerId,
      button: 0,
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

  beforeEach(() => {
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
});
