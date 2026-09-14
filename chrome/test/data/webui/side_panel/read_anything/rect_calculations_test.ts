// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {calculateTextBounds, ContentBrowserProxyImpl, getLineForRect, getMostCommonPitch, getRectIndexAtY, isRectMostlyVisible, isRectVisible, MOSTLY_VISIBLE_PERCENT, VisualBrowserProxyImpl} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setWindowSize} from './common.js';
import {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('RectCalculations', () => {
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.scrollTo(0, 0);
    visualBrowserProxy = new TestVisualBrowserProxy();
    VisualBrowserProxyImpl.setInstance(visualBrowserProxy);
    ContentBrowserProxyImpl.setInstance(new TestContentBrowserProxy());
  });

  suite('isRectVisible', () => {
    let windowHeight: number;
    let halfHeight: number;

    setup(() => {
      windowHeight = 600;
      halfHeight = windowHeight / 2;
      setWindowSize(windowHeight, 1000);
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
      windowHeight = 600;
      majorityHeight = windowHeight * MOSTLY_VISIBLE_PERCENT;
      minorityHeight = windowHeight - majorityHeight;
      setWindowSize(windowHeight, 1000);
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

    // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to be
    // gone.
    function getVisualLines(container: HTMLElement): string[] {
      const walker = document.createTreeWalker(
          container,
          NodeFilter.SHOW_TEXT,
          null,
      );
      const range = document.createRange();
      const lines: string[] = [];
      let currentLine = '';
      let lastTop: number|null = null;

      let node = walker.nextNode();
      while (node) {
        const text = node.textContent || '';
        for (let i = 0; i < text.length; i++) {
          range.setStart(node, i);
          range.setEnd(node, i + 1);
          const rect = range.getBoundingClientRect();
          if (rect.width === 0 && rect.height === 0) {
            continue;
          }
          if (lastTop === null) {
            lastTop = rect.top;
          } else if (Math.abs(rect.top - lastTop) > 2) {
            lines.push(currentLine);
            currentLine = '';
            lastTop = rect.top;
          }
          currentLine += text[i];
        }
        node = walker.nextNode();
      }
      if (currentLine) {
        lines.push(currentLine);
      }
      return lines;
    }

    // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to be
    // gone.
    function logBoundsFailure(
        testName: string, container: HTMLElement, expectedLength: number,
        bounds: DOMRect[]) {
      const rect = container.getBoundingClientRect();
      const visualLines = getVisualLines(container);
      console.error(
          `[${testName}] text bounds length is ${bounds.length}, expected ${
              expectedLength}.\n` +
          `Container: width=${container.offsetWidth}px, height=${
              container.offsetHeight}px, ` +
          `rect=[left:${rect.left}, top:${rect.top}, width:${
              rect.width}, height:${rect.height}], ` +
          `computedStyle.whiteSpace="${
              window.getComputedStyle(container).whiteSpace}"\n` +
          `Window: innerWidth=${window.innerWidth}px, body.clientWidth=${
              document.body.clientWidth}px\n` +
          `Visual lines (${visualLines.length}):\n` +
          visualLines.map((line, i) => `  [${i}]: "${line}"`).join('\n') +
          '\n' +
          `HTML: ${container.outerHTML}\n` +
          `Bounds: ${JSON.stringify(bounds.map(b => ({
                                                 top: b.top,
                                                 bottom: b.bottom,
                                                 left: b.left,
                                                 right: b.right,
                                               })))}`);
    }

    setup(() => {
      container = document.createElement('div');
      container.style.whiteSpace = 'pre';
      container.style.width = 'max-content';
      container.style.margin = '0';
      container.style.fontSize = '20px';
      container.style.lineHeight = '2';
      document.body.appendChild(container);
    });

    test('simple text returns bounds', () => {
      container.textContent = 'Hello world';
      visualBrowserProxy.fontSize = 12;
      visualBrowserProxy.lineSpacing = visualBrowserProxy.standardLineSpacing;

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

      visualBrowserProxy.fontSize = 10;
      visualBrowserProxy.lineSpacing = visualBrowserProxy.standardLineSpacing;

      const result = calculateTextBounds(container, 500);

      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      if (result.bounds.length !== 2) {
        logBoundsFailure(
            'moderate overlap does not merge with large font size', container,
            2, result.bounds);
      }
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

          visualBrowserProxy.fontSize = 1;
          visualBrowserProxy.lineSpacing =
              visualBrowserProxy.veryLooseLineSpacing;

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

      visualBrowserProxy.fontSize = 5;
      visualBrowserProxy.lineSpacing = visualBrowserProxy.standardLineSpacing;

      const result = calculateTextBounds(container, 500);

      // TODO(crbug.com/502069860): Remove this once flakiness is confirmed to
      // be gone.
      if (result.bounds.length !== 2) {
        logBoundsFailure(
            'small line height prevents merging for moderate overlap',
            container, 2, result.bounds);
      }
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

      visualBrowserProxy.fontSize = 2;
      visualBrowserProxy.lineSpacing = visualBrowserProxy.veryLooseLineSpacing;

      const result = calculateTextBounds(container, 500);

      assertEquals(1, result.bounds.length);
    });
  });

  suite('getMostCommonPitch', () => {
    test('empty bounds returns 0', () => {
      assertEquals(0, getMostCommonPitch([]));
    });

    test('single bound returns its rounded height', () => {
      const rect = new DOMRect(0, 0, 100, 20.44);
      assertEquals(20.4, getMostCommonPitch([rect]));
    });

    test('multiple bounds with uniform pitch returns that pitch', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 40, 100, 20);
      const rect3 = new DOMRect(0, 70, 100, 20);
      assertEquals(30, getMostCommonPitch([rect1, rect2, rect3]));
    });

    test('multiple bounds with mixed pitches returns most common', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 40, 100, 20);   // pitch 30
      const rect3 = new DOMRect(0, 70, 100, 20);   // pitch 30
      const rect4 = new DOMRect(0, 110, 100, 20);  // pitch 40
      assertEquals(30, getMostCommonPitch([rect1, rect2, rect3, rect4]));
    });

    test('floating point pitches are rounded to 1 decimal place', () => {
      const rect1 = new DOMRect(0, 10.01, 100, 20);
      const rect2 = new DOMRect(0, 40.02, 100, 20);   // pitch 30.01 -> 30.0
      const rect3 = new DOMRect(0, 70.03, 100, 20);   // pitch 30.01 -> 30.0
      const rect4 = new DOMRect(0, 100.05, 100, 20);  // pitch 30.02 -> 30.0
      assertEquals(30, getMostCommonPitch([rect1, rect2, rect3, rect4]));
    });
  });

  suite('getRectIndexAtY', () => {
    const rect1 = {bottom: 30} as DOMRect;
    const rect2 = {bottom: 60} as DOMRect;
    const rects = [rect1, rect2];

    test('Y matches first rect returns 0', () => {
      assertEquals(0, getRectIndexAtY(15, rects, true));
      assertEquals(0, getRectIndexAtY(15, rects, false));
    });

    test('Y matches second rect returns 1', () => {
      assertEquals(1, getRectIndexAtY(45, rects, true));
      assertEquals(0, getRectIndexAtY(45, rects, false));
    });

    test('Y in between rects with isForward=true returns current', () => {
      assertEquals(1, getRectIndexAtY(35, rects, true));
    });

    test('Y in between rects with isForward=false returns previous', () => {
      assertEquals(0, getRectIndexAtY(35, rects, false));
    });

    test('Y before all rects returns first index', () => {
      assertEquals(0, getRectIndexAtY(0, rects, true));
      assertEquals(0, getRectIndexAtY(0, rects, false));
    });

    test('Y past all rects returns last index', () => {
      assertEquals(1, getRectIndexAtY(65, rects, true));
      assertEquals(1, getRectIndexAtY(65, rects, false));
    });
  });

  suite('getLineForRect', () => {
    const line1 = new DOMRect(0, 10, 100, 20);  // top: 10, bottom: 30
    const line2 = new DOMRect(0, 40, 100, 20);  // top: 40, bottom: 60
    const line3 = new DOMRect(0, 70, 100, 20);  // top: 70, bottom: 90
    const lines = [line1, line2, line3];

    test('empty lines returns null', () => {
      const word = new DOMRect(0, 10, 20, 20);
      assertNull(getLineForRect(word, []));
    });

    test('word inside line returns containing line', () => {
      const wordLine1 = new DOMRect(0, 12, 30, 16);
      assertEquals(line1, getLineForRect(wordLine1, lines));

      const wordLine2 = new DOMRect(0, 42, 30, 16);
      assertEquals(line2, getLineForRect(wordLine2, lines));

      const wordLine3 = new DOMRect(0, 72, 30, 16);
      assertEquals(line3, getLineForRect(wordLine3, lines));
    });

    test('superscript at start of line returns containing line', () => {
      // Superscript on line 2 with raised baseline (smaller height, top above
      // line2.top) Line 1 is [10, 30], line 2 is [40, 60]. Superscript word:
      // top 35, height 12 (bottom 47, center 41). Overlaps line 2 [40, 47] by
      // 7px. No overlap with line 1 (bottom 30 < 35).
      const superscriptLine2 = new DOMRect(0, 35, 10, 12);
      assertEquals(line2, getLineForRect(superscriptLine2, lines));
    });

    test('subscript at start of line returns containing line', () => {
      // Subscript on line 2 with lowered baseline (smaller height, bottom below
      // line2.bottom). Line 2 is [40, 60], line 3 is [70, 90]. Subscript word:
      // top 53, height 12 (bottom 65, center 59). Overlaps line 2 [53, 60] by
      // 7px. No overlap with line 3 (bottom 65 < 70).
      const subscriptLine2 = new DOMRect(0, 53, 10, 12);
      assertEquals(line2, getLineForRect(subscriptLine2, lines));
    });

    test(
        'word partially overlapping adjacent lines picks maximum overlap',
        () => {
          const tightLine1 = new DOMRect(0, 10, 100, 20);  // [10, 30]
          const tightLine2 = new DOMRect(0, 30, 100, 20);  // [30, 50]
          // Word is [28, 48]: overlaps tightLine1 by 2px [28, 30] and
          // tightLine2 by 18px [30, 48].
          const word = new DOMRect(0, 28, 20, 20);
          assertEquals(
              tightLine2, getLineForRect(word, [tightLine1, tightLine2]));
        });

    test('word between lines falls back to line with closest midpoint', () => {
      // line1 midpoint = 20, line2 midpoint = 50.
      // Word falling in the gap [30, 40] with no overlap:
      // Word 1: [31, 33] (center 32). Distance to line1 is 12, to line2 is 18.
      const wordNearLine1 = new DOMRect(0, 31, 20, 2);
      assertEquals(line1, getLineForRect(wordNearLine1, lines));

      // Word 2: [37, 39] (center 38). Distance to line1 is 18, to line2 is 12.
      const wordNearLine2 = new DOMRect(0, 37, 20, 2);
      assertEquals(line2, getLineForRect(wordNearLine2, lines));
    });
  });
});
