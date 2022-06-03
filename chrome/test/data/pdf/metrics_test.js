// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType, record, recordFitTo, resetForTesting, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

chrome.test.runTests(function() {
  'use strict';

  class MockMetricsPrivate {
    constructor() {
      this.MetricTypeType = {HISTOGRAM_LOG: 'test_histogram_log'};
      this.actionCounter = {};
    }

    recordValue(metric, value) {
      chrome.test.assertEq('PDF.Actions', metric.metricName);
      chrome.test.assertEq('test_histogram_log', metric.type);
      chrome.test.assertEq(1, metric.min);
      chrome.test.assertEq(UserAction.NUMBER_OF_ACTIONS, metric.max);
      chrome.test.assertEq(UserAction.NUMBER_OF_ACTIONS + 1, metric.buckets);
      this.actionCounter[value] = (this.actionCounter[value] + 1) || 1;
    }
  }

  return [
    function testMetricsDocumentOpened() {
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();

      record(UserAction.DOCUMENT_OPENED);

      chrome.test.assertEq(
          {[UserAction.DOCUMENT_OPENED]: 1},
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    // Test that for every UserAction.<action> recorded an equivalent
    // UserAction.<action>_FIRST is recorded only once.
    function testMetricsFirstRecorded() {
      resetForTesting();
      chrome.metricsPrivate = new MockMetricsPrivate();

      const keys = Object.keys(UserAction).filter(key => {
        return key !== 'DOCUMENT_OPENED' && key !== 'NUMBER_OF_ACTIONS' &&
            !key.endsWith('_FIRST');
      });

      for (const key of keys) {
        const firstKey = `${key}_FIRST`;
        chrome.test.assertEq(
            chrome.metricsPrivate.actionCounter[firstKey], null);
        chrome.test.assertEq(chrome.metricsPrivate.actionCounter[key], null);
        record(UserAction[key]);
        record(UserAction[key]);
        chrome.test.assertEq(
            chrome.metricsPrivate.actionCounter[UserAction[firstKey]], 1);
        chrome.test.assertEq(
            chrome.metricsPrivate.actionCounter[UserAction[key]], 2);
      }
      chrome.test.assertEq(
          Object.keys(chrome.metricsPrivate.actionCounter).length,
          keys.length * 2);
      chrome.test.succeed();
    },

    function testMetricsFitTo() {
      resetForTesting();
      chrome.metricsPrivate = new MockMetricsPrivate();

      record(UserAction.DOCUMENT_OPENED);
      recordFitTo(FittingType.FIT_TO_HEIGHT);
      recordFitTo(FittingType.FIT_TO_PAGE);
      recordFitTo(FittingType.FIT_TO_WIDTH);
      recordFitTo(FittingType.FIT_TO_PAGE);
      recordFitTo(FittingType.FIT_TO_WIDTH);
      recordFitTo(FittingType.FIT_TO_PAGE);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.FIT_TO_PAGE_FIRST]: 1,
            [UserAction.FIT_TO_PAGE]: 3,
            [UserAction.FIT_TO_WIDTH_FIRST]: 1,
            [UserAction.FIT_TO_WIDTH]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },
  ];
}());
