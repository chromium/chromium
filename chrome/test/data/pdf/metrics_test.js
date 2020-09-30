// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/constants.js';
import {PDFMetrics, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/metrics.js';

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
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();

      PDFMetrics.record(UserAction.DOCUMENT_OPENED);

      chrome.test.assertEq(
          {[UserAction.DOCUMENT_OPENED]: 1},
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsRotation() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);
      for (let i = 0; i < 4; i++) {
        PDFMetrics.record(UserAction.ROTATE);
      }

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.ROTATE_FIRST]: 1,
            [UserAction.ROTATE]: 4
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsFitTo() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_HEIGHT);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_PAGE);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_WIDTH);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_PAGE);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_WIDTH);
      PDFMetrics.recordFitTo(FittingType.FIT_TO_PAGE);

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

    function testMetricsTwoUpView() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);
      PDFMetrics.recordTwoUpViewEnabled(true);
      PDFMetrics.recordTwoUpViewEnabled(false);
      PDFMetrics.recordTwoUpViewEnabled(true);
      PDFMetrics.recordTwoUpViewEnabled(false);
      PDFMetrics.recordTwoUpViewEnabled(true);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.TWO_UP_VIEW_ENABLE_FIRST]: 1,
            [UserAction.TWO_UP_VIEW_ENABLE]: 3,
            [UserAction.TWO_UP_VIEW_DISABLE_FIRST]: 1,
            [UserAction.TWO_UP_VIEW_DISABLE]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsZoomAction() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);
      PDFMetrics.recordZoomAction(/*isZoomIn=*/ true);
      PDFMetrics.recordZoomAction(/*isZoomIn=*/ false);
      PDFMetrics.recordZoomAction(/*isZoomIn=*/ true);
      PDFMetrics.recordZoomAction(/*isZoomIn=*/ false);
      PDFMetrics.recordZoomAction(/*isZoomIn=*/ true);
      PDFMetrics.record(UserAction.ZOOM_CUSTOM);
      PDFMetrics.record(UserAction.ZOOM_CUSTOM);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.ZOOM_IN_FIRST]: 1,
            [UserAction.ZOOM_IN]: 3,
            [UserAction.ZOOM_OUT_FIRST]: 1,
            [UserAction.ZOOM_OUT]: 2,
            [UserAction.ZOOM_CUSTOM_FIRST]: 1,
            [UserAction.ZOOM_CUSTOM]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsBookmarks() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);

      PDFMetrics.record(UserAction.OPEN_BOOKMARKS_PANEL);
      PDFMetrics.record(UserAction.FOLLOW_BOOKMARK);
      PDFMetrics.record(UserAction.FOLLOW_BOOKMARK);

      PDFMetrics.record(UserAction.OPEN_BOOKMARKS_PANEL);
      PDFMetrics.record(UserAction.FOLLOW_BOOKMARK);
      PDFMetrics.record(UserAction.FOLLOW_BOOKMARK);
      PDFMetrics.record(UserAction.FOLLOW_BOOKMARK);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.OPEN_BOOKMARKS_PANEL_FIRST]: 1,
            [UserAction.OPEN_BOOKMARKS_PANEL]: 2,
            [UserAction.FOLLOW_BOOKMARK_FIRST]: 1,
            [UserAction.FOLLOW_BOOKMARK]: 5
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsPageSelector() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);

      PDFMetrics.record(UserAction.PAGE_SELECTOR_NAVIGATE);
      PDFMetrics.record(UserAction.PAGE_SELECTOR_NAVIGATE);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.PAGE_SELECTOR_NAVIGATE_FIRST]: 1,
            [UserAction.PAGE_SELECTOR_NAVIGATE]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsSideNav() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);

      PDFMetrics.record(UserAction.TOGGLE_SIDENAV);
      PDFMetrics.record(UserAction.TOGGLE_SIDENAV);
      PDFMetrics.record(UserAction.TOGGLE_SIDENAV);
      PDFMetrics.record(UserAction.SELECT_SIDENAV_OUTLINE);
      PDFMetrics.record(UserAction.SELECT_SIDENAV_THUMBNAILS);
      PDFMetrics.record(UserAction.SELECT_SIDENAV_THUMBNAILS);
      PDFMetrics.record(UserAction.THUMBNAIL_NAVIGATE);
      PDFMetrics.record(UserAction.THUMBNAIL_NAVIGATE);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.THUMBNAIL_NAVIGATE_FIRST]: 1,
            [UserAction.THUMBNAIL_NAVIGATE]: 2,
            [UserAction.TOGGLE_SIDENAV_FIRST]: 1,
            [UserAction.TOGGLE_SIDENAV]: 3,
            [UserAction.SELECT_SIDENAV_THUMBNAILS_FIRST]: 1,
            [UserAction.SELECT_SIDENAV_THUMBNAILS]: 2,
            [UserAction.SELECT_SIDENAV_OUTLINE_FIRST]: 1,
            [UserAction.SELECT_SIDENAV_OUTLINE]: 1
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },

    function testMetricsSaving() {
      PDFMetrics.resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      PDFMetrics.record(UserAction.DOCUMENT_OPENED);

      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_ORIGINAL_ONLY);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_ORIGINAL_ONLY);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_ORIGINAL);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_EDITED);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_ORIGINAL);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_ORIGINAL);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_EDITED);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_WITH_ANNOTATION);
      PDFMetrics.record(UserAction.SAVE);
      PDFMetrics.record(UserAction.SAVE_WITH_ANNOTATION);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.SAVE_FIRST]: 1,
            [UserAction.SAVE]: 9,
            [UserAction.SAVE_WITH_ANNOTATION_FIRST]: 1,
            [UserAction.SAVE_WITH_ANNOTATION]: 2,
            [UserAction.SAVE_ORIGINAL_ONLY_FIRST]: 1,
            [UserAction.SAVE_ORIGINAL_ONLY]: 2,
            [UserAction.SAVE_ORIGINAL_FIRST]: 1,
            [UserAction.SAVE_ORIGINAL]: 3,
            [UserAction.SAVE_EDITED_FIRST]: 1,
            [UserAction.SAVE_EDITED]: 2
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },
  ];
}());
