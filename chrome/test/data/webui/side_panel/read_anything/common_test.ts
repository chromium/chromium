// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCurrentSpeechRate, isRectVisible} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('Common', () => {
  setup(() => {
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  test('getCurrentSpeechRate rounds value to 1 decimal', () => {
    chrome.readingMode.speechRate = 1.1234567890;
    assertEquals(1.1, getCurrentSpeechRate());

    chrome.readingMode.speechRate = 0.912345678;
    assertEquals(0.9, getCurrentSpeechRate());

    chrome.readingMode.speechRate = 1.199999999;
    assertEquals(1.2, getCurrentSpeechRate());
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
  });
});
