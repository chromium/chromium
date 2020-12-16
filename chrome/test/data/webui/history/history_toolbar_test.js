// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, ensureLazyLoaded} from 'chrome://history/history.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryEntry, createHistoryInfo} from 'chrome://test/history/test_util.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('history-toolbar', function() {
  let app;
  let element;
  let toolbar;
  let testService;
  const TEST_HISTORY_RESULTS =
      [createHistoryEntry('2016-03-15', 'https://google.com')];

  setup(function() {
    document.body.innerHTML = '';
    testService = new TestBrowserService();
    BrowserService.instance_ = testService;

    app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    toolbar = app.$.toolbar;
    return Promise
        .all([
          ensureLazyLoaded(),
          testService.whenCalled('queryHistory'),
        ])
        .then(flushTasks);
  });

  test('selecting checkbox causes toolbar to change', function() {
    testService.setQueryResult(
        {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
    element.fire('query-history', true);
    return testService.whenCalled('queryHistoryContinuation')
        .then(flushTasks)
        .then(function() {
          const item = element.$$('history-item');
          item.$.checkbox.click();

          // Ensure that when an item is selected that the count held by the
          // toolbar increases.
          assertEquals(1, toolbar.count);
          // Ensure that the toolbar boolean states that at least one item is
          // selected.
          assertTrue(toolbar.itemsSelected_);

          item.$.checkbox.click();

          // Ensure that when an item is deselected the count held by the
          // toolbar decreases.
          assertEquals(0, toolbar.count);
          // Ensure that the toolbar boolean states that no items are selected.
          assertFalse(toolbar.itemsSelected_);
        });
  });

  test('search term gathered correctly from toolbar', function() {
    testService.resetResolver('queryHistory');
    testService.setQueryResult(
        {info: createHistoryInfo('Test'), value: TEST_HISTORY_RESULTS});
    toolbar.$$('cr-toolbar').fire('search-changed', 'Test');
    return testService.whenCalled('queryHistory').then(query => {
      assertEquals('Test', query);
    });
  });

  test('spinner is active on search', function() {
    testService.resetResolver('queryHistory');
    testService.delayQueryResult();
    testService.setQueryResult({
      info: createHistoryInfo('Test2'),
      value: TEST_HISTORY_RESULTS,
    });
    toolbar.$$('cr-toolbar').fire('search-changed', 'Test2');
    return testService.whenCalled('queryHistory')
        .then(flushTasks)
        .then(() => {
          assertTrue(toolbar.spinnerActive);
          testService.finishQueryHistory();
        })
        .then(flushTasks)
        .then(() => {
          assertFalse(toolbar.spinnerActive);
        });
  });
});
