// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to verify that the chrome.histograms API is available
// and functioning correctly in WebUI. The tests here do not make any assertions
// but instead trigger the API calls which are then verified by the C++ test.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ChromeHistogramsTest', () => {
  test('All', () => {
    assertTrue(!!chrome.histograms);
    assertTrue(!!chrome.histograms.recordUserAction);
    assertTrue(!!chrome.histograms.recordBoolean);
    assertTrue(!!chrome.histograms.recordPercentage);
    assertTrue(!!chrome.histograms.recordSmallCount);
    assertTrue(!!chrome.histograms.recordMediumCount);
    assertTrue(!!chrome.histograms.recordCount);
    assertTrue(!!chrome.histograms.recordTime);
    assertTrue(!!chrome.histograms.recordMediumTime);
    assertTrue(!!chrome.histograms.recordLongTime);
    assertTrue(!!chrome.histograms.recordValue);
    assertTrue(!!chrome.histograms.recordEnumerationValue);
    assertTrue(!!chrome.histograms.recordSparseValue);

    chrome.histograms.recordUserAction('Test.ComputedAction');
    chrome.histograms.recordBoolean('Test.Boolean', true);
    chrome.histograms.recordPercentage('Test.Percentage', 50);
    chrome.histograms.recordSmallCount('Test.Counts100', 10);
    chrome.histograms.recordMediumCount('Test.Counts10000', 100);
    chrome.histograms.recordCount('Test.Counts1M', 1000);
    chrome.histograms.recordTime('Test.Times', 100);
    chrome.histograms.recordMediumTime('Test.MediumTimes', 1000);
    chrome.histograms.recordLongTime('Test.LongTimes', 10000);

    const metric: chrome.histograms.MetricType = {
      metricName: 'Test.CustomCounts',
      type: 'histogram-log' as chrome.histograms.MetricTypeType,
      min: 1,
      max: 100,
      buckets: 10,
    };
    chrome.histograms.recordValue(metric, 10);

    const metricLinear: chrome.histograms.MetricType = {
      metricName: 'Test.ExactLinear',
      type: 'histogram-linear' as chrome.histograms.MetricTypeType,
      min: 1,
      max: 10,
      buckets: 11,
    };
    chrome.histograms.recordValue(metricLinear, 5);

    chrome.histograms.recordEnumerationValue('Test.Enumeration', 1, 10);
    chrome.histograms.recordSparseValue('Test.Sparse', 10);
  });
});
