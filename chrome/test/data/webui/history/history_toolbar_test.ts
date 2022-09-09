// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {BrowserServiceImpl, ensureLazyLoaded, HistoryAppElement, HistoryEntry} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo} from './test_util.js';

suite('history-toolbar', function() {
  let app: HistoryAppElement;
  let testService: TestBrowserService;
  const TEST_HISTORY_RESULTS: [HistoryEntry] =
      [createHistoryEntry('2016-03-15', 'https://google.com')];

  setup(function() {
    document.body.innerHTML = '';
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    app = document.createElement('history-app');
    document.body.appendChild(app);
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
    app.$.history.dispatchEvent(new CustomEvent(
        'query-history', {bubbles: true, composed: true, detail: true}));
    return testService.whenCalled('queryHistoryContinuation')
        .then(flushTasks)
        .then(function() {
          const item = app.$.history.shadowRoot!.querySelector('history-item')!;
          item.$.checkbox.click();

          const toolbar = app.$.toolbar;

          // Ensure that when an item is selected that the count held by the
          // toolbar increases.
          assertEquals(1, toolbar.count);
          assertTrue(toolbar.$.mainToolbar.hasAttribute('has-overlay'));

          item.$.checkbox.click();

          // Ensure that when an item is deselected the count held by the
          // toolbar decreases.
          assertEquals(0, toolbar.count);
          assertFalse(toolbar.$.mainToolbar.hasAttribute('has-overlay'));
        });
  });

  test('search term gathered correctly from toolbar', function() {
    testService.resetResolver('queryHistory');
    const toolbar = app.$.toolbar;
    testService.setQueryResult(
        {info: createHistoryInfo('Test'), value: TEST_HISTORY_RESULTS});
    toolbar.$.mainToolbar.dispatchEvent(new CustomEvent(
        'search-changed', {bubbles: true, composed: true, detail: 'Test'}));
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
    const toolbar = app.$.toolbar;
    toolbar.$.mainToolbar.dispatchEvent(new CustomEvent(
        'search-changed', {bubbles: true, composed: true, detail: 'Test2'}));
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
