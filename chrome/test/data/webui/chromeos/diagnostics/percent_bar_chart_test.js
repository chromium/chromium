// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/percent_bar_chart.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.m.js';


suite('PercentBarChartTest', () => {
  /** @type {?HTMLElement} */
  let percentBarChartElement = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (percentBarChartElement) {
      percentBarChartElement.remove();
    }
    percentBarChartElement = null;
  });

  /**
   * @param {string} title
   * @param {number} value
   * @param {number} max
   */
  function initializePercentBarChart(title, value, max) {
    assertFalse(!!percentBarChartElement);

    // Add the element to the DOM.
    percentBarChartElement = document.createElement('percent-bar-chart');
    assertTrue(!!percentBarChartElement);
    document.body.appendChild(percentBarChartElement);
    percentBarChartElement.title = title;
    percentBarChartElement.value = value;
    percentBarChartElement.max = max;
    return flushTasks();
  }

  test('InitializePercentBarChart', () => {
    const title = 'Test title';
    const value = 10;
    const max = 30;
    const percent = Math.round(100 * value / max);
    return initializePercentBarChart(title, value, max).then(() => {
      const paperProgress = percentBarChartElement.$$('paper-progress');
      assertEquals(value, paperProgress.value);
      assertEquals(max, paperProgress.max);

      assertEquals(
          title, percentBarChartElement.$$('#chartName').textContent.trim());
      assertEquals(
          `${percent}`,
          percentBarChartElement.$$('#percentageLabel').textContent.trim());
    });
  });
});
