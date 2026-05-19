// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {getClickSourceType, getContextMenuSourceType} from 'chrome://webui-toolbar.top-chrome/app.js';

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
});
