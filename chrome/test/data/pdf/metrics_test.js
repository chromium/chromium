// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType, record, recordFitTo, recordTwoUpViewEnabled, recordZoomAction, resetForTesting, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

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

    function testMetricsRotation() {
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);
      for (let i = 0; i < 4; i++) {
        record(UserAction.ROTATE);
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

    function testMetricsTwoUpView() {
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);
      recordTwoUpViewEnabled(true);
      recordTwoUpViewEnabled(false);
      recordTwoUpViewEnabled(true);
      recordTwoUpViewEnabled(false);
      recordTwoUpViewEnabled(true);

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
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);
      recordZoomAction(/*isZoomIn=*/ true);
      recordZoomAction(/*isZoomIn=*/ false);
      recordZoomAction(/*isZoomIn=*/ true);
      recordZoomAction(/*isZoomIn=*/ false);
      recordZoomAction(/*isZoomIn=*/ true);
      record(UserAction.ZOOM_CUSTOM);
      record(UserAction.ZOOM_CUSTOM);

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
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);

      record(UserAction.OPEN_BOOKMARKS_PANEL);
      record(UserAction.FOLLOW_BOOKMARK);
      record(UserAction.FOLLOW_BOOKMARK);

      record(UserAction.OPEN_BOOKMARKS_PANEL);
      record(UserAction.FOLLOW_BOOKMARK);
      record(UserAction.FOLLOW_BOOKMARK);
      record(UserAction.FOLLOW_BOOKMARK);

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
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);

      record(UserAction.PAGE_SELECTOR_NAVIGATE);
      record(UserAction.PAGE_SELECTOR_NAVIGATE);

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
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);

      record(UserAction.TOGGLE_SIDENAV);
      record(UserAction.TOGGLE_SIDENAV);
      record(UserAction.TOGGLE_SIDENAV);
      record(UserAction.SELECT_SIDENAV_OUTLINE);
      record(UserAction.SELECT_SIDENAV_THUMBNAILS);
      record(UserAction.SELECT_SIDENAV_THUMBNAILS);
      record(UserAction.THUMBNAIL_NAVIGATE);
      record(UserAction.THUMBNAIL_NAVIGATE);

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
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);

      record(UserAction.SAVE);
      record(UserAction.SAVE_ORIGINAL_ONLY);
      record(UserAction.SAVE);
      record(UserAction.SAVE_ORIGINAL_ONLY);
      record(UserAction.SAVE);
      record(UserAction.SAVE_ORIGINAL);
      record(UserAction.SAVE);
      record(UserAction.SAVE_EDITED);
      record(UserAction.SAVE);
      record(UserAction.SAVE_ORIGINAL);
      record(UserAction.SAVE);
      record(UserAction.SAVE_ORIGINAL);
      record(UserAction.SAVE);
      record(UserAction.SAVE_EDITED);
      record(UserAction.SAVE);
      record(UserAction.SAVE_WITH_ANNOTATION);
      record(UserAction.SAVE);
      record(UserAction.SAVE_WITH_ANNOTATION);

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

    function testMetricsOverflowMenu() {
      resetForTesting();

      chrome.metricsPrivate = new MockMetricsPrivate();
      record(UserAction.DOCUMENT_OPENED);

      record(UserAction.TOGGLE_DISPLAY_ANNOTATIONS);
      record(UserAction.PRESENT);
      record(UserAction.PROPERTIES);
      record(UserAction.PRESENT);
      record(UserAction.PRESENT);
      record(UserAction.PROPERTIES);
      record(UserAction.TOGGLE_DISPLAY_ANNOTATIONS);
      record(UserAction.PROPERTIES);
      record(UserAction.PRESENT);

      chrome.test.assertEq(
          {
            [UserAction.DOCUMENT_OPENED]: 1,
            [UserAction.TOGGLE_DISPLAY_ANNOTATIONS_FIRST]: 1,
            [UserAction.TOGGLE_DISPLAY_ANNOTATIONS]: 2,
            [UserAction.PRESENT_FIRST]: 1,
            [UserAction.PRESENT]: 4,
            [UserAction.PROPERTIES_FIRST]: 1,
            [UserAction.PROPERTIES]: 3,
          },
          chrome.metricsPrivate.actionCounter);
      chrome.test.succeed();
    },
  ];
}());
