// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryInfo, navigateTo} from './test_util.js';

suite('routing-with-query-param', function() {
  let app: HistoryAppElement;
  let expectedQuery: string;
  let testService: TestBrowserService;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;

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

    embeddingsHandler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(embeddingsHandler));
    embeddingsHandler.setResultFor(
        'search', Promise.resolve({result: {items: []}}));

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

  test('search with after date', async () => {
    // Wait for initial query to get called.
    await testService.whenCalled('queryHistory');
    testService.reset();

    loadTimeData.overrideValues({enableHistoryEmbeddings: true});

    const expectedDate = new Date('2011-04-05');
    expectedDate.setHours(0, 0, 0, 0);
    const expectedTimestamp = expectedDate.getTime();

    navigateTo('/?q=query&after=2011-04-05', app);
    const [query, timestamp] = await testService.whenCalled('queryHistory');
    assertEquals(expectedQuery, query);
    assertEquals(expectedTimestamp, timestamp);
  });

  test('invalidates wrongly formatted dates', async () => {
    // Wait for initial query to get called.
    await testService.whenCalled('queryHistory');
    testService.reset();

    loadTimeData.overrideValues({enableHistoryEmbeddings: true});

    // Invalid date format should only query the search term.
    navigateTo('/?q=hello', app);
    const searchTerm = await testService.whenCalled('queryHistory');
    assertEquals('hello', searchTerm);
  });
});
