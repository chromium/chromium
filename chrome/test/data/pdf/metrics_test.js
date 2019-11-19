// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFMetrics} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/metrics.js';
import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_fitting_type.js';

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
      chrome.test.assertEq(PDFMetrics.UserAction.NUMBER_OF_ACTIONS, metric.max);
      chrome.test.assertEq(
          PDFMetrics.UserAction.NUMBER_OF_ACTIONS + 1, metric.buckets);
      this.actionCounter[value] = (this.actionCounter[value] + 1) || 1;
    }
  }

  return [
    function testMetricsDocumentOpened() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();

      PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);

      chrome.test.assertEq(
          {[PDFMetrics.UserAction.DOCUMENT_OPENED]: 1},
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsRotation() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);
      for (let i = 0; i < 4; i++) {
        PDFMetrics.record(PDFMetrics.UserAction.ROTATE);
      }

      chrome.test.assertEq(
          {
            [PDFMetrics.UserAction.DOCUMENT_OPENED]: 1,
            [PDFMetrics.UserAction.ROTATE_FIRST]: 1,
            [PDFMetrics.UserAction.ROTATE]: 4
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsFitTo() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_HEIGHT);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_PAGE);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_WIDTH);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_PAGE);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_WIDTH);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_PAGE);

      chrome.test.assertEq(
          {
            [PDFMetrics.UserAction.DOCUMENT_OPENED]: 1,
            [PDFMetrics.UserAction.FIT_TO_PAGE_FIRST]: 1,
            [PDFMetrics.UserAction.FIT_TO_PAGE]: 3,
            [PDFMetrics.UserAction.FIT_TO_WIDTH_FIRST]: 1,
            [PDFMetrics.UserAction.FIT_TO_WIDTH]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsBookmarks() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);

      PDFMetrics.record(PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL);
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);

      PDFMetrics.record(PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL);
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);

      chrome.test.assertEq(
          {
            [PDFMetrics.UserAction.DOCUMENT_OPENED]: 1,
            [PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL_FIRST]: 1,
            [PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL]: 2,
            [PDFMetrics.UserAction.FOLLOW_BOOKMARK_FIRST]: 1,
            [PDFMetrics.UserAction.FOLLOW_BOOKMARK]: 5
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsPageSelector() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);

      PDFMetrics.record(PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE);
      PDFMetrics.record(PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE);

      chrome.test.assertEq(
          {
            [PDFMetrics.UserAction.DOCUMENT_OPENED]: 1,
            [PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE_FIRST]: 1,
            [PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },
  ];
}());
