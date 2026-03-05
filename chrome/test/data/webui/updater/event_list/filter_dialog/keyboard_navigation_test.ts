// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {handleKeyboardNavigation} from 'chrome://updater/event_list/filter_dialog/keyboard_navigation.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('handleKeyboardNavigation', () => {
  let items: HTMLElement[];

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    items = [
      document.createElement('button'),
      document.createElement('button'),
      document.createElement('button'),
    ];
    items.forEach((item, index) => {
      item.textContent = `Item ${index}`;
      document.body.appendChild(item);
      item.addEventListener('keydown', e => handleKeyboardNavigation(e, items));
    });
    items[0]!.focus();
  });

  test('ArrowDown navigates to next item', () => {
    items[0]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));

    assertEquals(items[1], document.activeElement);
  });

  test('ArrowDown wraps around to first item', () => {
    items[2]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));

    assertEquals(items[0], document.activeElement);
  });

  test('ArrowUp navigates to previous item', () => {
    items[1]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));

    assertEquals(items[0], document.activeElement);
  });

  test('ArrowUp wraps around to last item', () => {
    items[0]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));

    assertEquals(items[2], document.activeElement);
  });

  test('Home navigates to first item', () => {
    items[2]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'Home'}));

    assertEquals(items[0], document.activeElement);
  });

  test('End navigates to last item', () => {
    items[0]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'End'}));

    assertEquals(items[2], document.activeElement);
  });

  test('Other keys are ignored', () => {
    items[0]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    assertEquals(items[0], document.activeElement);
  });
});
