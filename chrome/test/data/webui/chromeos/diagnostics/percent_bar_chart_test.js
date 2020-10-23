// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/percent_bar_chart.js';

import {flushTasks} from 'chrome://test/test_util.m.js';
import * as diagnostics_test_utils from './diagnostics_test_utils.js';

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
   * @param {string} header
   * @param {number} value
   * @param {number} max
   * @param {string=} headerIcon
   */
  function initializePercentBarChart(header, value, max, headerIcon) {
    assertFalse(!!percentBarChartElement);

    // Add the element to the DOM.
    percentBarChartElement = document.createElement('percent-bar-chart');
    assertTrue(!!percentBarChartElement);
    percentBarChartElement.header = header;
    percentBarChartElement.value = value;
    percentBarChartElement.max = max;
    percentBarChartElement.headerIcon = headerIcon || '';
    document.body.appendChild(percentBarChartElement);

    return flushTasks();
  }

  test('InitializePercentBarChart', () => {
    const header = 'Test header';
    const value = 10;
    const max = 30;
    const percent = Math.round(100 * value / max);
    return initializePercentBarChart(header, value, max).then(() => {
      const paperProgress = percentBarChartElement.$$('paper-progress');
      assertEquals(value, paperProgress.value);
      assertEquals(max, paperProgress.max);

      assertEquals(
          header, percentBarChartElement.$$('#chartName').textContent.trim());
      diagnostics_test_utils.assertElementContainsText(
          percentBarChartElement.$$('#percentageLabel'), `${percent}`);

      assertFalse(!!percentBarChartElement.$$('#headerIcon'));
    });
  });

  test('WithHeaderIcon', () => {
    const header = 'Test header';
    const value = 10;
    const max = 30;
    const icon = 'cr:warning';
    return initializePercentBarChart(header, value, max, icon).then(() => {
      assertEquals(icon, percentBarChartElement.headerIcon);
      assertTrue(!!percentBarChartElement.$$('#headerIcon'));
    });
  });
});
