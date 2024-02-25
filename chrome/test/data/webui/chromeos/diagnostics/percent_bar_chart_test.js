// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/percent_bar_chart.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {PercentBarChartElement} from 'chrome://diagnostics/percent_bar_chart.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('percentBarChartTestSuite', function() {
  /** @type {?PercentBarChartElement} */
  let percentBarChartElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
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
   */
  function initializePercentBarChart(header, value, max) {
    assertFalse(!!percentBarChartElement);

    // Add the element to the DOM.
    percentBarChartElement = /** @type {!PercentBarChartElement} */ (
        document.createElement('percent-bar-chart'));
    assertTrue(!!percentBarChartElement);
    percentBarChartElement.header = header;
    percentBarChartElement.value = value;
    percentBarChartElement.max = max;
    document.body.appendChild(percentBarChartElement);

    return flushTasks();
  }

  test('InitializePercentBarChart', () => {
    const header = 'Test header';
    const value = 10;
    const max = 30;
    const percent = Math.round(100 * value / max);
    return initializePercentBarChart(header, value, max).then(() => {
      const paperProgress =
          percentBarChartElement.shadowRoot.querySelector('paper-progress');
      assertEquals(value, paperProgress.value);
      assertEquals(max, paperProgress.max);

      assertEquals(
          header,
          percentBarChartElement.shadowRoot.querySelector('#chartName')
              .textContent.trim());
    });
  });

  test('ClampsToMaxValue', () => {
    const header = 'Test header';
    const value = 101;
    const max = 100;
    return initializePercentBarChart(header, value, max).then(() => {
      const paperProgress =
          percentBarChartElement.shadowRoot.querySelector('paper-progress');
      assertEquals(paperProgress.value, paperProgress.max);
    });
  });
});
