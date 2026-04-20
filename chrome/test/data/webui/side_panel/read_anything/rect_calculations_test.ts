// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {calculateTextBounds, isRectMostlyVisible, isRectVisible, MOSTLY_VISIBLE_PERCENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('RectCalculations', () => {
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

  suite('calculateTextBounds', () => {
    let container: HTMLDivElement;

    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      container = document.createElement('div');
      container.style.lineHeight = '1';
      container.style.margin = '0';
      container.style.padding = '0';
      document.body.appendChild(container);
    });

    test('simple text returns bounds', () => {
      container.textContent = 'Hello world';
      chrome.readingMode.fontSize = 12;
      chrome.readingMode.lineSpacing = chrome.readingMode.standardLineSpacing;

      const result = calculateTextBounds(container, 500);

      assertEquals(container.offsetTop, result.minY);
      assertEquals(500, result.maxY);
      assertTrue(result.bounds.length > 0);
    });

    test('empty container returns empty bounds', () => {
      const result = calculateTextBounds(container, 500);

      assertEquals(0, result.bounds.length);
    });

    test('moderate overlap does not merge with large font size', () => {
      const line1 = document.createElement('span');
      line1.textContent = 'Line 1';
      line1.style.position = 'absolute';
      line1.style.top = '0px';
      line1.style.display = 'inline';

      const line2 = document.createElement('span');
      line2.textContent = 'Line 2';
      line2.style.position = 'absolute';
      line2.style.top = '20px';
      line2.style.display = 'inline';

      container.appendChild(line1);
      container.appendChild(line2);

      chrome.readingMode.fontSize = 10;
      chrome.readingMode.lineSpacing = chrome.readingMode.standardLineSpacing;

      const result = calculateTextBounds(container, 500);

      assertEquals(2, result.bounds.length);
    });

    test(
        'small font size and large line spacing causes merging for small overlaps',
        () => {
          const line1 = document.createElement('span');
          line1.textContent = 'Line 1';
          line1.style.position = 'absolute';
          line1.style.top = '0px';
          line1.style.display = 'inline';

          const line2 = document.createElement('span');
          line2.textContent = 'Line 2';
          line2.style.position = 'absolute';
          line2.style.top = '2px';
          line2.style.display = 'inline';

          container.appendChild(line1);
          container.appendChild(line2);

          chrome.readingMode.fontSize = 1;
          chrome.readingMode.lineSpacing =
              chrome.readingMode.veryLooseLineSpacing;

          const result = calculateTextBounds(container, 500);

          assertEquals(1, result.bounds.length);
        });

    test('small line height prevents merging for moderate overlap', () => {
      const line1 = document.createElement('span');
      line1.textContent = 'Line 1';
      line1.style.position = 'absolute';
      line1.style.top = '0px';
      line1.style.display = 'inline';

      const line2 = document.createElement('span');
      line2.textContent = 'Line 2';
      line2.style.position = 'absolute';
      line2.style.top = '10px';
      line2.style.display = 'inline';

      container.appendChild(line1);
      container.appendChild(line2);

      chrome.readingMode.fontSize = 5;
      chrome.readingMode.lineSpacing = chrome.readingMode.standardLineSpacing;

      const result = calculateTextBounds(container, 500);

      assertEquals(2, result.bounds.length);
    });

    test('large line height causes merging for moderate overlap', () => {
      const line1 = document.createElement('span');
      line1.textContent = 'Line 1';
      line1.style.position = 'absolute';
      line1.style.top = '0px';
      line1.style.display = 'inline';

      const line2 = document.createElement('span');
      line2.textContent = 'Line 2';
      line2.style.position = 'absolute';
      line2.style.top = '0px';
      line2.style.display = 'inline';

      container.appendChild(line1);
      container.appendChild(line2);

      chrome.readingMode.fontSize = 2;
      chrome.readingMode.lineSpacing = chrome.readingMode.veryLooseLineSpacing;

      const result = calculateTextBounds(container, 500);

      assertEquals(1, result.bounds.length);
    });
  });
});
