// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService} from 'chrome://history/history.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryInfo} from 'chrome://test/history/test_util.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('routing-with-query-param', function() {
  let app;
  let toolbar;
  let expectedQuery;
  let testService;

  setup(function() {
    document.body.innerHTML = '';
    window.history.replaceState({}, '', '/?q=query');
    testService = new TestBrowserService();
    BrowserService.instance_ = testService;
    // Ignore the initial empty query so that we can correctly check the
    // search term for the second call to queryHistory().
    testService.ignoreNextQuery();

    testService.setQueryResult({
      info: createHistoryInfo('query'),
      value: [],
    });
    app = document.createElement('history-app');
    document.body.appendChild(app);
    toolbar = app.$['toolbar'];
    expectedQuery = 'query';
  });

  test('search initiated on load', function() {
    return testService.whenCalled('queryHistory')
        .then(query => {
          assertEquals(expectedQuery, query);
          return flushTasks();
        })
        .then(function() {
          assertEquals(
              expectedQuery,
              toolbar.$['main-toolbar'].getSearchField().getValue());
        });
  });
});
