// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getWordCount, isRectMostlyVisible, isRectVisible, MOSTLY_VISIBLE_PERCENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('Common', () => {
  setup(() => {
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  suite('isRectVisible', () => {
    let windowHeight: number;
    let halfHeight: number;

    setup(() => {
      windowHeight = document.documentElement.clientHeight;
      halfHeight = windowHeight / 2;
      window.innerHeight = windowHeight;
    });

    test('fully inside window returns true', () => {
      const rect = new DOMRect(0, 0, halfHeight, halfHeight);
      assertTrue(isRectVisible(rect));
    });

    test('bottom inside window returns true', () => {
      const rect =
          new DOMRect(-halfHeight, -halfHeight, windowHeight, windowHeight);
      assertTrue(isRectVisible(rect));
    });

    test('top inside window returns true', () => {
      const rect =
          new DOMRect(halfHeight, halfHeight, windowHeight, windowHeight);
      assertTrue(isRectVisible(rect));
    });

    test('bigger than window returns true', () => {
      const rect = new DOMRect(
          -halfHeight, -halfHeight, windowHeight * 2, windowHeight * 2);
      assertTrue(isRectVisible(rect));
    });

    test('fully above window returns false', () => {
      const rect = new DOMRect(-halfHeight, -halfHeight, -1, -1);
      assertFalse(isRectVisible(rect));
    });

    test('fully below window returns false', () => {
      const rect = new DOMRect(
          windowHeight + 1, windowHeight + 1, halfHeight, halfHeight);
      assertFalse(isRectVisible(rect));
    });

    test('empty bounds returns false', () => {
      const rect = new DOMRect();
      assertFalse(isRectVisible(rect));
    });
  });

  suite('isRectMostlyVisible', () => {
    let windowHeight: number;
    let majorityHeight: number;
    let minorityHeight: number;

    setup(() => {
      windowHeight = document.documentElement.clientHeight;
      majorityHeight = windowHeight * MOSTLY_VISIBLE_PERCENT;
      minorityHeight = windowHeight - majorityHeight;
      window.innerHeight = windowHeight;
    });

    test('fully inside window returns true', () => {
      const rect = new DOMRect(0, 0, majorityHeight, majorityHeight);
      assertTrue(isRectMostlyVisible(rect));
    });

    test('bottom mostly inside window returns true', () => {
      const rect = new DOMRect(
          -minorityHeight, -minorityHeight, windowHeight, windowHeight);
      assertTrue(isRectMostlyVisible(rect));
    });

    test('bottom mostly outside window returns false', () => {
      const rect = new DOMRect(
          -majorityHeight, -majorityHeight, windowHeight, windowHeight);
      assertFalse(isRectMostlyVisible(rect));
    });

    test('top mostly inside window returns true', () => {
      const rect = new DOMRect(
          minorityHeight, minorityHeight, windowHeight, windowHeight);
      assertTrue(isRectMostlyVisible(rect));
    });

    test('top mostly outside window returns false', () => {
      const rect = new DOMRect(
          majorityHeight, majorityHeight, windowHeight, windowHeight);
      assertFalse(isRectMostlyVisible(rect));
    });

    test('slightly bigger than window returns true', () => {
      const top = -minorityHeight / 2;
      const height = windowHeight + minorityHeight;
      const rect = new DOMRect(top, top, height, height);
      assertTrue(isRectMostlyVisible(rect));
    });

    test('much bigger than window returns false', () => {
      const top = -majorityHeight / 2;
      const height = windowHeight + majorityHeight;
      const rect = new DOMRect(top, top, height, height);
      assertFalse(isRectMostlyVisible(rect));
    });

    test('fully above window returns false', () => {
      const rect = new DOMRect(-majorityHeight, -majorityHeight, -1, -1);
      assertFalse(isRectMostlyVisible(rect));
    });

    test('fully below window returns false', () => {
      const rect = new DOMRect(
          windowHeight + 1, windowHeight + 1, majorityHeight, majorityHeight);
      assertFalse(isRectMostlyVisible(rect));
    });

    test('empty bounds returns false', () => {
      const rect = new DOMRect();
      assertFalse(isRectMostlyVisible(rect));
    });
  });

  test('getWordCount', () => {
    assertEquals(5, getWordCount('Nothing but the truth now'));
    assertEquals(8, getWordCount('Nothing but the\nproof of\n what I am'));
    assertEquals(1, getWordCount('TheworstofwhatIcamefrom'));
    assertEquals(
        4, getWordCount('patterns           I\'m ashamed\n\n\n\n\n\n   of'));
  });
});
