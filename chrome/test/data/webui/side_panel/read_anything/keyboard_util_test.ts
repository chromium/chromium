// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {getNewIndex, isArrow, isForwardArrow, isHorizontalArrow} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('Keyboard utils', () => {
  test('isArrow', () => {
    assertTrue(isArrow('ArrowRight'));
    assertTrue(isArrow('ArrowUp'));
    assertTrue(isArrow('ArrowLeft'));
    assertTrue(isArrow('ArrowDown'));
    assertFalse(isArrow('not a key'));
    assertFalse(isArrow('w'));
    assertFalse(isArrow('Tab'));
    assertFalse(isArrow('Space'));
  });

  test('isForwardArrow ltr', () => {
    document.documentElement.dir = 'ltr';

    assertTrue(isForwardArrow('ArrowRight'));
    assertTrue(isForwardArrow('ArrowDown'));
    assertFalse(isForwardArrow('ArrowUp'));
    assertFalse(isForwardArrow('ArrowLeft'));
    assertFalse(isForwardArrow('not a key'));
    assertFalse(isForwardArrow('w'));
    assertFalse(isForwardArrow('Tab'));
    assertFalse(isForwardArrow('Space'));
  });

  test('isForwardArrow rtl', () => {
    document.documentElement.dir = 'rtl';

    assertFalse(isForwardArrow('ArrowRight'));
    assertFalse(isForwardArrow('ArrowDown'));
    assertTrue(isForwardArrow('ArrowUp'));
    assertTrue(isForwardArrow('ArrowLeft'));
    assertFalse(isForwardArrow('not a key'));
    assertFalse(isForwardArrow('w'));
    assertFalse(isForwardArrow('Tab'));
    assertFalse(isForwardArrow('Space'));
  });

  test('isHorizontalArrow', () => {
    assertTrue(isHorizontalArrow('ArrowRight'));
    assertTrue(isHorizontalArrow('ArrowLeft'));
    assertFalse(isHorizontalArrow('ArrowUp'));
    assertFalse(isHorizontalArrow('ArrowDown'));
    assertFalse(isHorizontalArrow('not a key'));
    assertFalse(isHorizontalArrow('w'));
    assertFalse(isHorizontalArrow('Tab'));
    assertFalse(isHorizontalArrow('Space'));
  });

  suite('getNewIndex', () => {
    let focusableElements: HTMLElement[];

    setup(() => {
      focusableElements = [
        document.createElement('p'),
        document.createElement('button'),
        document.createElement('div'),
        document.createElement('span'),
        document.createElement('button'),
        document.createElement('link'),
      ];
    });

    test('non-arrow key returns current index', () => {
      const expectedIndex = 2;
      const target = focusableElements[expectedIndex];

      assertTrue(!!target);
      assertEquals(expectedIndex, getNewIndex('s', target, focusableElements));
      assertEquals(
          expectedIndex, getNewIndex('fake key', target, focusableElements));
      assertEquals(
          expectedIndex, getNewIndex('Shift', target, focusableElements));
    });

    test('with ltr returns next element', () => {
      document.documentElement.dir = 'ltr';
      const index = 2;
      const target = focusableElements[index];

      assertTrue(!!target);
      assertEquals(
          index + 1, getNewIndex('ArrowRight', target, focusableElements));
      assertEquals(
          index + 1, getNewIndex('ArrowDown', target, focusableElements));
      assertEquals(
          index - 1, getNewIndex('ArrowLeft', target, focusableElements));
      assertEquals(
          index - 1, getNewIndex('ArrowUp', target, focusableElements));
    });

    test('with ltr wraps around', () => {
      document.documentElement.dir = 'ltr';

      let index = 0;
      let target = focusableElements[index];
      assertTrue(!!target);
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowLeft', target, focusableElements));
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowUp', target, focusableElements));

      index = focusableElements.length - 1;
      target = focusableElements[index];
      assertTrue(!!target);
      assertEquals(0, getNewIndex('ArrowRight', target, focusableElements));
      assertEquals(0, getNewIndex('ArrowDown', target, focusableElements));
    });

    test('with ltr and element is not in the list', () => {
      document.documentElement.dir = 'ltr';
      const target = document.createElement('a');

      assertTrue(!!target);
      assertEquals(0, getNewIndex('ArrowRight', target, focusableElements));
      assertEquals(0, getNewIndex('ArrowDown', target, focusableElements));
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowLeft', target, focusableElements));
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowUp', target, focusableElements));
    });

    test('with rtl returns next element', () => {
      document.documentElement.dir = 'rtl';
      const index = 2;
      const target = focusableElements[index];

      assertTrue(!!target);
      assertEquals(
          index - 1, getNewIndex('ArrowRight', target, focusableElements));
      assertEquals(
          index - 1, getNewIndex('ArrowDown', target, focusableElements));
      assertEquals(
          index + 1, getNewIndex('ArrowLeft', target, focusableElements));
      assertEquals(
          index + 1, getNewIndex('ArrowUp', target, focusableElements));
    });

    test('with rtl wraps around', () => {
      document.documentElement.dir = 'rtl';

      let index = 0;
      let target = focusableElements[index];
      assertTrue(!!target);
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowRight', target, focusableElements));
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowDown', target, focusableElements));

      index = focusableElements.length - 1;
      target = focusableElements[index];
      assertTrue(!!target);
      assertEquals(0, getNewIndex('ArrowLeft', target, focusableElements));
      assertEquals(0, getNewIndex('ArrowUp', target, focusableElements));
    });

    test('with rtl and element is not in the list', () => {
      document.documentElement.dir = 'rtl';
      const target = document.createElement('a');

      assertTrue(!!target);
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowRight', target, focusableElements));
      assertEquals(
          focusableElements.length - 1,
          getNewIndex('ArrowDown', target, focusableElements));
      assertEquals(0, getNewIndex('ArrowLeft', target, focusableElements));
      assertEquals(0, getNewIndex('ArrowUp', target, focusableElements));
    });
  });
});
