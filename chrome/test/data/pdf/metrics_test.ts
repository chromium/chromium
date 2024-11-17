// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType, record, recordFitTo, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {setupMockMetricsPrivate} from './test_util.js';

chrome.test.runTests(function() {
  'use strict';

  return [
    function testMetricsDocumentOpened() {
      const mockMetricsPrivate = setupMockMetricsPrivate();

      record(UserAction.DOCUMENT_OPENED);

      chrome.test.assertEq(1, mockMetricsPrivate.actionCounter.size);
      chrome.test.assertEq(
          1, mockMetricsPrivate.actionCounter.get(UserAction.DOCUMENT_OPENED));
      chrome.test.succeed();
    },

    // Test that for every UserAction.<action> recorded an equivalent
    // UserAction.<action>_FIRST is recorded only once.
    function testMetricsFirstRecorded() {
      const mockMetricsPrivate = setupMockMetricsPrivate();

      const keys = (Object.keys(UserAction) as Array<keyof typeof UserAction>)
                       .filter(key => Number.isInteger(UserAction[key]))
                       .filter(key => {
                         return key !== 'DOCUMENT_OPENED' &&
                             key !== 'NUMBER_OF_ACTIONS' &&
                             !key.endsWith('_FIRST');
                       });

      for (const key of keys) {
        const firstKey = `${key}_FIRST` as keyof typeof UserAction;
        chrome.test.assertEq(
            mockMetricsPrivate.actionCounter.get(UserAction[firstKey]), null);
        chrome.test.assertEq(
            mockMetricsPrivate.actionCounter.get(UserAction[key]), null);
        record(UserAction[key]);
        record(UserAction[key]);
        chrome.test.assertEq(
            mockMetricsPrivate.actionCounter.get(UserAction[firstKey]), 1);
        chrome.test.assertEq(
            mockMetricsPrivate.actionCounter.get(UserAction[key]), 2);
      }
      chrome.test.assertEq(
          mockMetricsPrivate.actionCounter.size, keys.length * 2);
      chrome.test.succeed();
    },

    function testMetricsFitTo() {
      const mockMetricsPrivate = setupMockMetricsPrivate();

      record(UserAction.DOCUMENT_OPENED);
      recordFitTo(FittingType.FIT_TO_HEIGHT);
      recordFitTo(FittingType.FIT_TO_PAGE);
      recordFitTo(FittingType.FIT_TO_WIDTH);
      recordFitTo(FittingType.FIT_TO_PAGE);
      recordFitTo(FittingType.FIT_TO_WIDTH);
      recordFitTo(FittingType.FIT_TO_PAGE);

      const expectations: Array<[UserAction, number]> = [
        [UserAction.DOCUMENT_OPENED, 1],
        [UserAction.FIT_TO_PAGE_FIRST, 1],
        [UserAction.FIT_TO_PAGE, 3],
        [UserAction.FIT_TO_WIDTH_FIRST, 1],
        [UserAction.FIT_TO_WIDTH, 2],
      ];
      chrome.test.assertEq(
          expectations.length, mockMetricsPrivate.actionCounter.size);

      expectations.forEach(([action, value]) => {
        chrome.test.assertEq(
            mockMetricsPrivate.actionCounter.get(action), value);
      });

      chrome.test.succeed();
    },
  ];
}());
