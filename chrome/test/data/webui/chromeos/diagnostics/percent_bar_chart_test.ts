// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/percent_bar_chart.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {PercentBarChartElement} from 'chrome://diagnostics/percent_bar_chart.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PaperProgressElement} from 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('percentBarChartTestSuite', function() {
  let percentBarChartElement: PercentBarChartElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    percentBarChartElement?.remove();
    percentBarChartElement = null;
  });

  function initializePercentBarChart(
      header: string, value: number, max: number): Promise<void> {
    // Add the element to the DOM.
    percentBarChartElement = document.createElement('percent-bar-chart');
    assert(percentBarChartElement);
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
    return initializePercentBarChart(header, value, max).then(() => {
      assert(percentBarChartElement);
      const paperProgress = percentBarChartElement.shadowRoot!.querySelector(
                                'paper-progress') as PaperProgressElement;
      assertEquals(value, paperProgress.value);
      assertEquals(max, paperProgress.max);
      const chartName = strictQuery(
          '#chartName', percentBarChartElement.shadowRoot, HTMLLabelElement);
      assertEquals(header, chartName.textContent!.trim());
    });
  });

  test('ClampsToMaxValue', () => {
    const header = 'Test header';
    const value = 101;
    const max = 100;
    return initializePercentBarChart(header, value, max).then(() => {
      assert(percentBarChartElement);
      const paperProgress = percentBarChartElement.shadowRoot!.querySelector(
                                'paper-progress') as PaperProgressElement;
      assertEquals(paperProgress.value, paperProgress.max);
    });
  });
});
