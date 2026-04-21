// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusLineStyleMode, LineFocusModel, LineFocusNoneStyleMode, LineFocusStyle, LineFocusWindowStyleMode, WINDOW_DIFF_THRESHOLD} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('LineFocusStyleMode', () => {
  let model: LineFocusModel;

  setup(() => {
    model = new LineFocusModel();
  });

  suite('line mode', () => {
    let mode: LineFocusLineStyleMode;
    const style = LineFocusStyle.UNDERLINE;

    setup(() => {
      mode = new LineFocusLineStyleMode(style, model);
    });

    test('getStyle returns style', () => {
      assertEquals(style, mode.getStyle());
    });

    test('calculateHeight sets top with no height', () => {
      const y = 100;
      model.setFocalPoint(y);

      mode.calculateHeight();

      assertEquals(y, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('getFocalPointForRect returns bottom', () => {
      const rect = new DOMRect(0, 10, 100, 20);
      assertEquals(30, mode.getFocalPointForRect(rect));
    });

    test('clampLineIndex returns index unchanged', () => {
      assertEquals(5, mode.clampLineIndex(5));
    });

    test('getFocusWindowBounds returns same rect for top and bottom', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const lines = [rect1, rect2];

      const bounds = mode.getFocusWindowBounds(lines, 1);

      assertEquals(rect2, bounds.topRect);
      assertEquals(rect2, bounds.bottomRect);
    });

    test('getDesiredCenter returns bottomRect.bottom', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const lines = [rect1, rect2];

      assertEquals(50, mode.getDesiredCenter(lines, 1));
    });

    test('shouldRefreshFocalPoint returns true', () => {
      assertTrue(mode.shouldRefreshFocalPoint(0, 0));
    });
  });

  suite('window mode', () => {
    let largeMode: LineFocusWindowStyleMode;
    let smallMode: LineFocusWindowStyleMode;
    const largeStyle = LineFocusStyle.MEDIUM_WINDOW;
    const smallStyle = LineFocusStyle.SMALL_WINDOW;

    setup(() => {
      largeMode = new LineFocusWindowStyleMode(largeStyle, model);
      smallMode = new LineFocusWindowStyleMode(smallStyle, model);
    });

    test('calculateHeight with empty bounds does nothing', () => {
      model.setTextBounds([]);

      largeMode.calculateHeight();

      assertEquals(0, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('calculateHeight sets top and height', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);  // bottom 30
      const rect2 = new DOMRect(0, 30, 100, 20);  // bottom 50
      const rect3 = new DOMRect(0, 50, 100, 20);  // bottom 70
      model.setTextBounds([rect1, rect2, rect3]);
      model.setCurrentLineIndex(1);

      largeMode.calculateHeight();

      // With 3 lines, top index is 1 - (3-1)/2 = 0.
      // Top should be rect1.top = 10.
      assertEquals(10, model.getTop());
      // Height should be rect3.bottom - top = 70 - 10 = 60.
      assertEquals(60, model.getWindowHeight());
    });

    test('calculateHeight clamps top index at boundaries', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);

      // Try to focus on line 0. Top index would be 0 - 1 = -1, clamped to 0.
      model.setCurrentLineIndex(0);
      largeMode.calculateHeight();
      assertEquals(10, model.getTop());

      // Try to focus on line 2. Top index would be 2 - 1 = 1.
      // bottom index is 1 + 2 = 3 (out of bounds, clamped to 2).
      model.setCurrentLineIndex(2);
      largeMode.calculateHeight();
      // validTopIndex should be 0 because maxTopIndex = 3 - 3 = 0.
      assertEquals(10, model.getTop());
    });

    test(
        'calculateHeight with window larger than available lines clamps to last line',
        () => {
          const rect1 = new DOMRect(0, 10, 100, 20);  // bottom 30
          model.setTextBounds([rect1]);
          model.setCurrentLineIndex(0);

          largeMode.calculateHeight();

          assertEquals(10, model.getTop());
          assertEquals(20, model.getWindowHeight());
        });

    test('calculateHeight small window sets top and height', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);  // bottom 30
      const rect2 = new DOMRect(0, 30, 100, 20);  // bottom 50
      model.setTextBounds([rect1, rect2]);
      model.setCurrentLineIndex(1);

      smallMode.calculateHeight();

      assertEquals(30, model.getTop());
      assertEquals(20, model.getWindowHeight());
    });

    test('calculateHeight clamps to top visible rect', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      model.setTextBounds([rect1, rect2]);
      model.setCurrentLineIndex(0);
      // Set minY so first line is not visible.
      model.setMinY(20);

      largeMode.calculateHeight();

      // The findIndex should find rect2 (top 30 >= 20).
      assertEquals(30, model.getTop());
    });

    test('getFocalPointForRect returns center', () => {
      const rect = new DOMRect(0, 10, 100, 20);
      assertEquals(20, largeMode.getFocalPointForRect(rect));
    });

    test('clampLineIndex clamps index such that window stays centered', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);

      // The center of the window should be index 1.
      assertEquals(1, largeMode.clampLineIndex(0));
      assertEquals(1, largeMode.clampLineIndex(1));
      assertEquals(1, largeMode.clampLineIndex(2));
    });

    test('clampLineIndex small window returns same index', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      model.setTextBounds([rect1, rect2, rect3]);

      assertEquals(0, smallMode.clampLineIndex(0));
      assertEquals(1, smallMode.clampLineIndex(1));
      assertEquals(2, smallMode.clampLineIndex(2));
    });

    test('getFocusWindowBounds returns span of lines', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      const lines = [rect1, rect2, rect3];

      const bounds = largeMode.getFocusWindowBounds(lines, 1);

      assertEquals(rect1, bounds.topRect);
      assertEquals(rect3, bounds.bottomRect);
    });

    test('getFocusWindowBounds small window returns same line', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      const lines = [rect1, rect2, rect3];

      const bounds = smallMode.getFocusWindowBounds(lines, 1);

      assertEquals(rect2, bounds.topRect);
      assertEquals(rect2, bounds.bottomRect);
    });

    test('getDesiredCenter returns center of window', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      const lines = [rect1, rect2, rect3];

      assertEquals(
          (rect2.top + rect2.bottom) / 2, largeMode.getDesiredCenter(lines, 1));
    });

    test('getDesiredCenter small window returns center of window', () => {
      const rect1 = new DOMRect(0, 10, 100, 20);
      const rect2 = new DOMRect(0, 30, 100, 20);
      const rect3 = new DOMRect(0, 50, 100, 20);
      const lines = [rect1, rect2, rect3];

      assertEquals(
          (rect2.top + rect2.bottom) / 2, smallMode.getDesiredCenter(lines, 1));
    });

    test('shouldRefreshFocalPoint respects threshold', () => {
      model.setWindowHeight(100);
      model.setTop(50);

      // Diff within threshold
      assertFalse(largeMode.shouldRefreshFocalPoint(
          100 + WINDOW_DIFF_THRESHOLD, 50 + WINDOW_DIFF_THRESHOLD));

      // Diff outside threshold
      assertTrue(largeMode.shouldRefreshFocalPoint(
          100 + WINDOW_DIFF_THRESHOLD + 1, 50));
      assertTrue(largeMode.shouldRefreshFocalPoint(
          100, 50 + WINDOW_DIFF_THRESHOLD + 1));
    });
  });

  suite('off mode', () => {
    let mode: LineFocusNoneStyleMode;
    const style = LineFocusStyle.OFF;

    setup(() => {
      mode = new LineFocusNoneStyleMode(style, model);
    });

    test('calculateHeight does nothing', () => {
      mode.calculateHeight();
      assertEquals(0, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('getFocalPointForRect returns 0', () => {
      const rect = new DOMRect(0, 10, 100, 20);
      assertEquals(0, mode.getFocalPointForRect(rect));
    });

    test('clampLineIndex returns 0', () => {
      assertEquals(0, mode.clampLineIndex(5));
    });

    test('getFocusWindowBounds returns empty rects', () => {
      const bounds = mode.getFocusWindowBounds([], 0);
      assertEquals(0, bounds.topRect.top);
      assertEquals(0, bounds.bottomRect.bottom);
    });

    test('getDesiredCenter returns 0', () => {
      assertEquals(0, mode.getDesiredCenter([], 0));
    });

    test('shouldRefreshFocalPoint returns false', () => {
      assertFalse(mode.shouldRefreshFocalPoint(100, 100));
    });
  });
});
