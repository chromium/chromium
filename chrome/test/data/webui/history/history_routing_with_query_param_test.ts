// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserProxyImpl} from 'chrome://history/history.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryInfo} from './test_util.js';

suite('routing-with-query-param', function() {
  let app: HistoryAppElement;
  let expectedQuery: string;
  let testProxy: TestHistoryBrowserProxy;

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
});
