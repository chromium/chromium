// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';

import {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('CrMenuSelectorFocusTest', () => {
  let element: CrMenuSelector;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-menu-selector') as CrMenuSelector;

    // Slot some menu items.
    for (let i = 0; i < 3; i++) {
      const item = document.createElement('button');
      item.setAttribute('role', 'menuitem');
      item.id = i.toString();
      element.appendChild(item);
    }

    document.body.appendChild(element);
  });

  function getChild(index: number): HTMLButtonElement {
    return (element.children as HTMLCollectionOf<HTMLButtonElement>)[index]!;
  }

  test('ArrowKeysMoveFocus', () => {
    // The focus is not in any of the children yet, so the first arrow down
    // should focus the first menu item.
    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(0), getDeepActiveElement());

    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(1), getDeepActiveElement());

    keyDownOn(getChild(1), 0, [], 'ArrowUp');
    assertEquals(getChild(0), getDeepActiveElement());
  });

  test('HomeMovesFocusToFirstElement', () => {
    getChild(0).focus();
    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    keyDownOn(getChild(2), 0, [], 'Home');
    assertEquals(getChild(0), getDeepActiveElement());
  });

  test('EndMovesFocusToFirstElement', () => {
    getChild(0).focus();
    keyDownOn(getChild(2), 0, [], 'End');
    assertEquals(getChild(2), getDeepActiveElement());
  });

  test('WrapsFocusWhenReachingEnds', () => {
    getChild(0).focus();
    keyDownOn(getChild(0), 0, [], 'ArrowUp');
    assertEquals(getChild(2), getDeepActiveElement());

    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(0), getDeepActiveElement());
  });

  test('SkipsDisabledElements', () => {
    getChild(0).focus();
    getChild(1).disabled = true;
    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(2), getDeepActiveElement());
  });

  test('SkipsHiddenElements', () => {
    getChild(0).focus();
    getChild(1).hidden = true;
    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(2), getDeepActiveElement());
  });

  test('SkipsNonMenuItems', () => {
    getChild(0).focus();
    getChild(1).setAttribute('role', 'presentation');
    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(2), getDeepActiveElement());
  });

  test('FocusingIntoByKeyboardAlwaysFocusesFirstItem', () => {
    const outsideElement = document.createElement('button');
    document.body.appendChild(outsideElement);
    outsideElement.focus();

    // Mock document as having been focused by keyboard.
    FocusOutlineManager.forDocument(document).visible = true;

    getChild(2).focus();
    assertEquals(getChild(0), getDeepActiveElement());
  });

  test('FocusingIntoByClickDoesNotFocusFirstItem', () => {
    const outsideElement = document.createElement('button');
    document.body.appendChild(outsideElement);
    outsideElement.focus();

    // Mock document as not having been focused by keyboard.
    FocusOutlineManager.forDocument(document).visible = false;

    getChild(2).focus();
    assertEquals(getChild(2), getDeepActiveElement());
  });

  test('TabMovesFocusToLastElement', async () => {
    getChild(0).focus();

    const tabEventPromise = eventToPromise('keydown', getChild(0));
    keyDownOn(getChild(0), 0, [], 'Tab');
    const tabEvent = await tabEventPromise;
    assertEquals(getChild(2), getDeepActiveElement());
    assertFalse(tabEvent.defaultPrevented);
  });

  test('ShiftTabMovesFocusToFirstElement', async () => {
    // First, mock focus on last element.
    getChild(0).focus();
    keyDownOn(getChild(0), 0, [], 'End');

    const shiftTabEventPromise = eventToPromise('keydown', getChild(2));
    keyDownOn(getChild(2), 0, ['shift'], 'Tab');
    const shiftTabEvent = await shiftTabEventPromise;
    assertEquals(getChild(0), getDeepActiveElement());
    assertFalse(shiftTabEvent.defaultPrevented);
  });
});
