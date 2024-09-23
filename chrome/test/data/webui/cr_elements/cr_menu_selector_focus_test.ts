// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';

import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';


suite('CrMenuSelectorFocusTest', () => {
  let element: CrMenuSelector;

  setup(async () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-menu-selector attr-for-selected="href" selected-attribute="selected"
          selectable="[selectable]">
        <a role="menuitem" href="/a" selectable>a</a>
        <a role="menuitem" href="/b" selectable>b</a>
        <a role="menuitem" href="/c" selectable>c</a>
        <a role="menuitem" href="/d">d</a>
      </cr-menu-selector>
    `;
    element = document.querySelector('cr-menu-selector')!;
    await element.updateComplete;
  });

  function getChild(index: number): HTMLAnchorElement {
    return (element.children as HTMLCollectionOf<HTMLAnchorElement>)[index]!;
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
    assertEquals(getChild(3), getDeepActiveElement());
  });

  test('WrapsFocusWhenReachingEnds', () => {
    getChild(0).focus();
    keyDownOn(getChild(0), 0, [], 'ArrowUp');
    assertEquals(getChild(3), getDeepActiveElement());

    keyDownOn(getChild(0), 0, [], 'ArrowDown');
    assertEquals(getChild(0), getDeepActiveElement());
  });

  test('SkipsDisabledElements', () => {
    getChild(0).focus();
    getChild(1).toggleAttribute('disabled', true);
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
    assertEquals(getChild(3), getDeepActiveElement());
    assertFalse(tabEvent.defaultPrevented);
  });

  test('ShiftTabMovesFocusToFirstElement', async () => {
    // First, mock focus on last element.
    getChild(0).focus();
    keyDownOn(getChild(0), 0, [], 'End');
    await microtasksFinished();

    const shiftTabEventPromise = eventToPromise('keydown', getChild(2));
    keyDownOn(getChild(2), 0, ['shift'], 'Tab');
    const shiftTabEvent = await shiftTabEventPromise;
    assertEquals(getChild(0), getDeepActiveElement());
    assertFalse(shiftTabEvent.defaultPrevented);
  });

  test('SetsSelectedItemUsingHrefAttribute', async () => {
    const firstItem = getChild(0);
    element.selected = firstItem.getAttribute('href')!;
    await microtasksFinished();
    assertTrue(firstItem.hasAttribute('selected'));
    assertEquals('page', firstItem.getAttribute('aria-current'));
    const secondItem = getChild(1);
    element.selected = secondItem.getAttribute('href')!;
    await microtasksFinished();
    assertFalse(firstItem.hasAttribute('selected'));
    assertFalse(firstItem.hasAttribute('aria-current'));
    assertTrue(secondItem.hasAttribute('selected'));
    assertEquals('page', secondItem.getAttribute('aria-current'));
  });

  test('DoesNotSelectUnselectableItems', async () => {
    assertEquals(3, element.getItemsForTest().length);
    element.selected = 'http://google.com';
    await microtasksFinished();
    assertFalse(getChild(3).hasAttribute('selected'));
  });

  test('ActivatesItemOnClick', async () => {
    const itemToSelect = getChild(1);
    const onActivate = eventToPromise('iron-activate', element);
    const onSelect = eventToPromise('iron-select', element);
    itemToSelect.dispatchEvent(new Event('click', {bubbles: true}));
    await Promise.all([onActivate, onSelect]);
    assertTrue(itemToSelect.hasAttribute('selected'));
    assertEquals(itemToSelect.getAttribute('href'), element.selected);
  });
});
