// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserProxyImpl, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryInfo, navigateTo} from './test_util.js';

suite('routing-with-query-param', function() {
  let app: HistoryAppElement;
  let expectedQuery: string;
  let testProxy: TestHistoryBrowserProxy;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState({}, '', '/?q=query');
    testProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);

    testProxy.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo('query'),
        value: [],
      },
    }));

    embeddingsHandler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(embeddingsHandler));
    embeddingsHandler.setResultFor(
        'search', Promise.resolve({result: {items: []}}));

    app = document.createElement('history-app');
    document.body.appendChild(app);
    expectedQuery = 'query';
  });

  test('search initiated on load', async function() {
    const query = await testProxy.handler.whenCalled('queryHistory');
    assertEquals(expectedQuery, query[0]);
    await microtasksFinished();
    assertEquals(
        expectedQuery, app.$.toolbar.$.mainToolbar.getSearchField().getValue());
  });

  test('search with after date', async () => {
    // Wait for initial query to get called.
    await testProxy.handler.whenCalled('queryHistory');
    testProxy.handler.reset();

    loadTimeData.overrideValues({enableHistoryEmbeddings: true});

    const expectedDate = new Date('2011-04-05');
    expectedDate.setHours(0, 0, 0, 0);
    const expectedTimestamp = expectedDate.getTime();

    testProxy.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo(''),
        value: [],
      },
    }));

    navigateTo('/?q=query&after=2011-04-05', app);
    const [query, numResults, timestamp] =
        await testProxy.handler.whenCalled('queryHistory');
    assertEquals(expectedQuery, query);
    assertEquals(numResults, 150);
    assertEquals(expectedTimestamp, timestamp);
  });

  test('invalidates wrongly formatted dates', async () => {
    // Wait for initial query to get called.
    await testProxy.handler.whenCalled('queryHistory');
    testProxy.handler.reset();

    loadTimeData.overrideValues({enableHistoryEmbeddings: true});

    testProxy.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo(''),
        value: [],
      },
    }));

    navigateTo('/?q=query&after=some-bad-date', app);
    const [query, numResults, timestamp] =
        await testProxy.handler.whenCalled('queryHistory');
    assertEquals('query', query);
    assertEquals(numResults, 150);
    assertEquals(null, timestamp);
  });
});
