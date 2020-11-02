// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertFalse} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

suite('CrMenuSelectorFocusTest', () => {
  /** @type {!CrMenuSelector} */
  let element;

  setup(() => {
    document.body.innerHTML = '';
    element = document.createElement('cr-menu-selector');

    // Slot some menu items.
    for (let i = 0; i < 3; i++) {
      const item = document.createElement('button');
      item.setAttribute('role', 'menuitem');
      item.id = i;
      element.appendChild(item);
    }

    document.body.appendChild(element);
  });

  test('ArrowKeysMoveFocus', () => {
    // The focus is not in any of the children yet, so the first arrow down
    // should focus the first menu item.
    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    assertEquals(element.children[0], getDeepActiveElement());

    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    assertEquals(element.children[1], getDeepActiveElement());

    keyDownOn(element.children[1], 0, [], 'ArrowUp');
    assertEquals(element.children[0], getDeepActiveElement());
  });

  test('HomeMovesFocusToFirstElement', () => {
    element.children[0].focus();
    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    keyDownOn(element.children[2], 0, [], 'Home');
    assertEquals(element.children[0], getDeepActiveElement());
  });

  test('EndMovesFocusToFirstElement', () => {
    element.children[0].focus();
    keyDownOn(element.children[2], 0, [], 'End');
    assertEquals(element.children[2], getDeepActiveElement());
  });

  test('WrapsFocusWhenReachingEnds', () => {
    element.children[0].focus();
    keyDownOn(element.children[0], 0, [], 'ArrowUp');
    assertEquals(element.children[2], getDeepActiveElement());

    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    assertEquals(element.children[0], getDeepActiveElement());
  });

  test('SkipsDisabledElements', () => {
    element.children[0].focus();
    element.children[1].disabled = true;
    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    assertEquals(element.children[2], getDeepActiveElement());
  });

  test('SkipsHiddenElements', () => {
    element.children[0].focus();
    element.children[1].hidden = true;
    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    assertEquals(element.children[2], getDeepActiveElement());
  });

  test('SkipsNonMenuItems', () => {
    element.children[0].focus();
    element.children[1].setAttribute('role', 'presentation');
    keyDownOn(element.children[0], 0, [], 'ArrowDown');
    assertEquals(element.children[2], getDeepActiveElement());
  });

  test('FocusingIntoByKeyboardAlwaysFocusesFirstItem', () => {
    const outsideElement = document.createElement('button');
    document.body.appendChild(outsideElement);
    outsideElement.focus();

    // Mock document as having been focused by keyboard.
    FocusOutlineManager.forDocument(document).visible = true;

    element.children[2].focus();
    assertEquals(element.children[0], getDeepActiveElement());
  });

  test('FocusingIntoByClickDoesNotFocusFirstItem', () => {
    const outsideElement = document.createElement('button');
    document.body.appendChild(outsideElement);
    outsideElement.focus();

    // Mock document as not having been focused by keyboard.
    FocusOutlineManager.forDocument(document).visible = false;

    element.children[2].focus();
    assertEquals(element.children[2], getDeepActiveElement());
  });

  test('TabMovesFocusToLastElement', async () => {
    element.children[0].focus();

    const tabEventPromise = eventToPromise('keydown', element.children[0]);
    keyDownOn(element.children[0], 0, [], 'Tab');
    const tabEvent = await tabEventPromise;
    assertEquals(element.children[2], getDeepActiveElement());
    assertFalse(tabEvent.defaultPrevented);
  });

  test('ShiftTabMovesFocusToFirstElement', async () => {
    // First, mock focus on last element.
    element.children[0].focus();
    keyDownOn(element.children[0], 0, [], 'End');

    const shiftTabEventPromise = eventToPromise('keydown', element.children[2]);
    keyDownOn(element.children[2], 0, ['shift'], 'Tab');
    const shiftTabEvent = await shiftTabEventPromise;
    assertEquals(element.children[0], getDeepActiveElement());
    assertFalse(shiftTabEvent.defaultPrevented);
  });
});