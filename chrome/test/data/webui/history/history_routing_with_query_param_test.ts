// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {BrowserServiceImpl, HistoryAppElement} from 'chrome://history/history.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryInfo} from './test_util.js';

suite('routing-with-query-param', function() {
  let app: HistoryAppElement;
  let expectedQuery: string;
  let testService: TestBrowserService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState({}, '', '/?q=query');
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    // Ignore the initial empty query so that we can correctly check the
    // search term for the second call to queryHistory().
    testService.ignoreNextQuery();

    testService.setQueryResult({
      info: createHistoryInfo('query'),
      value: [],
    });
    app = document.createElement('history-app');
    document.body.appendChild(app);
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
              app.$.toolbar.$.mainToolbar.getSearchField().getValue());
        });
  });
});
