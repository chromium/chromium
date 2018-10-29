// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// metrics api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Metrics

// Any changes to the logging done in these functions should be matched
// with the checks done in IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Metrics).
// See metrics_apitest.cc.
chrome.test.runTests([
  function getIsCrashReportingEnabled() {
    chrome.metricsPrivate.getIsCrashReportingEnabled(function(enabled) {
      chrome.test.assertEq('boolean', typeof enabled);
      chrome.test.succeed();
    });
  },

  function recordUserAction() {
    // Log a metric once.
    chrome.metricsPrivate.recordUserAction('test.ua.1');

    // Log a metric more than once.
    chrome.metricsPrivate.recordUserAction('test.ua.2');
    chrome.metricsPrivate.recordUserAction('test.ua.2');

    chrome.test.succeed();
  },

  function recordValue() {
    chrome.metricsPrivate.recordValue({
      'metricName': 'test.h.1',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
      'min': 1,
      'max': 100,
      'buckets': 50
    }, 42);

    chrome.metricsPrivate.recordValue({
      'metricName': 'test.h.2',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
      'min': 1,
      'max': 200,
      'buckets': 50
    }, 42);

    chrome.metricsPrivate.recordPercentage('test.h.3', 42);
    chrome.metricsPrivate.recordPercentage('test.h.3', 42);

    chrome.test.succeed();
  },

  function recordSparseValue() {
    chrome.metricsPrivate.recordSparseValue('test.sparse.1', 42);
    chrome.metricsPrivate.recordSparseValue('test.sparse.2', 24);
    chrome.metricsPrivate.recordSparseValue('test.sparse.2', 24);
    chrome.metricsPrivate.recordSparseValue('test.sparse.3', 1);
    chrome.metricsPrivate.recordSparseValue('test.sparse.3', 2);
    chrome.metricsPrivate.recordSparseValue('test.sparse.3', 2);
    chrome.metricsPrivate.recordSparseValue('test.sparse.3', 3);
    chrome.metricsPrivate.recordSparseValue('test.sparse.3', 3);
    chrome.metricsPrivate.recordSparseValue('test.sparse.3', 3);

    chrome.test.succeed();
  },

  function recordTimes() {
    chrome.metricsPrivate.recordTime('test.time', 42);
    chrome.metricsPrivate.recordMediumTime('test.medium.time', 42 * 1000);
    chrome.metricsPrivate.recordLongTime('test.long.time', 42 * 1000 * 60);

    chrome.test.succeed();
  },

  function recordCounts() {
    chrome.metricsPrivate.recordCount('test.count', 420000);
    chrome.metricsPrivate.recordMediumCount('test.medium.count', 4200);
    chrome.metricsPrivate.recordSmallCount('test.small.count', 42);

    chrome.test.succeed();
  },

  function getFieldTrial() {
    var test1Callback = function(group) {
      chrome.test.assertEq('', group);
      chrome.metricsPrivate.getFieldTrial('apitestfieldtrial2', test2Callback);
    };

    var test2Callback = function(group) {
      chrome.test.assertEq('group1', group);
      chrome.test.succeed();
    };

    chrome.metricsPrivate.getFieldTrial('apitestfieldtrial1', test1Callback);
  },

  function getVariationParams1() {
    chrome.metricsPrivate.getVariationParams(
        'apitestfieldtrial1', function(params) {
      chrome.test.assertEq(undefined, chrome.runtime.lastError);
      chrome.test.assertEq(undefined, params);
      chrome.test.succeed();
    });
  },

  function getVariationParams2() {
    chrome.metricsPrivate.getVariationParams(
        'apitestfieldtrial2', function(params) {
      chrome.test.assertEq(undefined, chrome.runtime.lastError);
      chrome.test.assertEq({a: 'aa', b: 'bb'}, params);
      chrome.test.succeed();
    });
  },

  function testBucketSizeChanges() {
    var linear1 = {
      'metricName': 'test.bucketchange.linear',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
      'min': 0,
      'max': 100,
      'buckets': 10
    };
    var linear2 = {
      'metricName': 'test.bucketchange.linear',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
      'min': 0,
      'max': 100,
      'buckets': 20
    };
    var log1 = {
      'metricName': 'test.bucketchange.log',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
      'min': 0,
      'max': 100,
      'buckets': 10
    };
    var log2 = {
      'metricName': 'test.bucketchange.log',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
      'min': 0,
      'max': 100,
      'buckets': 20
    };

    chrome.metricsPrivate.recordValue(linear1, 42);
    // This one should be rejected because the bucket count is different.
    // We check for sample count == 2 in metrics_apitest.cc
    chrome.metricsPrivate.recordValue(linear2, 42);
    chrome.metricsPrivate.recordValue(linear1, 42);

    chrome.metricsPrivate.recordValue(log1, 42);
    // This one should be rejected because the bucket count is different.
    // We check for sample count == 2 in metrics_apitest.cc
    chrome.metricsPrivate.recordValue(log2, 42);
    chrome.metricsPrivate.recordValue(log1, 42);

    chrome.test.succeed();
  },

]);

