// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Scrollbar} from 'chrome://sys-internals/line_chart/scrollbar.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertCloseTo} from '../test_util.js';

suite('LineChart_Scrollbar', function() {
  test('Scrollbar integration test', function() {
    const scrollbar = new Scrollbar(function() {});
    scrollbar.resize(100);
    scrollbar.setRange(1000);

    /* See |Scrollbar.isScrolledToRightEdge()|. */
    const scrollError = 2;
    assertFalse(scrollbar.isScrolledToRightEdge());
    assertCloseTo(scrollbar.getPosition(), 0, scrollError);
    scrollbar.scrollToRightEdge();
    assertTrue(scrollbar.isScrolledToRightEdge());
    assertCloseTo(scrollbar.getPosition(), 1000, scrollError);
    scrollbar.setPosition(500);
    assertFalse(scrollbar.isScrolledToRightEdge());
    assertCloseTo(scrollbar.getPosition(), 500, scrollError);
    scrollbar.setRange(100);
    assertTrue(scrollbar.isScrolledToRightEdge());
    assertCloseTo(scrollbar.getPosition(), 100, scrollError);
  });
});
