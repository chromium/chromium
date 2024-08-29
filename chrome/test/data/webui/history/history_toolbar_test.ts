// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement, HistoryEntry} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo} from './test_util.js';

suite('history-toolbar', function() {
  let app: HistoryAppElement;
  let testService: TestBrowserService;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;
  const TEST_HISTORY_RESULTS: [HistoryEntry] =
      [createHistoryEntry('2016-03-15', 'https://google.com')];

  function createToolbar() {
    const toolbar = document.createElement('history-toolbar');
    document.body.appendChild(toolbar);
    return toolbar;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    embeddingsHandler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(embeddingsHandler));
    embeddingsHandler.setResultFor(
        'search', Promise.resolve({result: {items: []}}));

    app = document.createElement('history-app');
    document.body.appendChild(app);
    return Promise
        .all([
          ensureLazyLoaded(),
          testService.whenCalled('queryHistory'),
        ])
        .then(flushTasks);
  });

  test('selecting checkbox causes toolbar to change', async function() {
    testService.setQueryResult(
        {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
    app.$.history.dispatchEvent(new CustomEvent(
        'query-history', {bubbles: true, composed: true, detail: true}));
    await testService.whenCalled('queryHistoryContinuation');
    await flushTasks();
    const item = app.$.history.shadowRoot!.querySelector('history-item')!;
    item.$.checkbox.click();
    await item.$.checkbox.updateComplete;

    const toolbar = app.$.toolbar;

    // Ensure that when an item is selected that the count held by the
    // toolbar increases.
    assertEquals(1, toolbar.count);
    assertTrue(toolbar.$.mainToolbar.hasAttribute('has-overlay'));

    item.$.checkbox.click();
    await item.$.checkbox.updateComplete;

    // Ensure that when an item is deselected the count held by the
    // toolbar decreases.
    assertEquals(0, toolbar.count);
    assertFalse(toolbar.$.mainToolbar.hasAttribute('has-overlay'));
  });

  test('search term gathered correctly from toolbar', async function() {
    testService.resetResolver('queryHistory');
    const toolbar = app.$.toolbar;
    testService.setQueryResult(
        {info: createHistoryInfo('Test'), value: TEST_HISTORY_RESULTS});
    toolbar.$.mainToolbar.dispatchEvent(new CustomEvent(
        'search-changed', {bubbles: true, composed: true, detail: 'Test'}));
    const query = await testService.whenCalled('queryHistory');
    assertEquals('Test', query);
  });

  test('spinner is active on search', async function() {
    testService.resetResolver('queryHistory');
    testService.delayQueryResult();
    testService.setQueryResult({
      info: createHistoryInfo('Test2'),
      value: TEST_HISTORY_RESULTS,
    });
    const toolbar = app.$.toolbar;
    toolbar.$.mainToolbar.dispatchEvent(new CustomEvent(
        'search-changed', {bubbles: true, composed: true, detail: 'Test2'}));
    await testService.whenCalled('queryHistory');
    await flushTasks();
    assertTrue(toolbar.spinnerActive);
    testService.finishQueryHistory();
    await flushTasks();
    assertFalse(toolbar.spinnerActive);
  });

  test('updates search icon', async () => {
    function createToolbar() {
      const toolbar = document.createElement('history-toolbar');
      document.body.appendChild(toolbar);
      return toolbar;
    }

    // Without history embeddings enabled, search icon should always be default.
    loadTimeData.overrideValues({enableHistoryEmbeddings: false});
    let toolbar = createToolbar();
    await flushTasks();
    toolbar.selectedPage = 'history';
    assertEquals(undefined, toolbar.$.mainToolbar.searchIconOverride);

    // With history embeddings enabled, search icon should change.
    loadTimeData.overrideValues({enableHistoryEmbeddings: true});
    toolbar = createToolbar();
    await flushTasks();
    toolbar.selectedPage = 'history';
    assertEquals(
        'history-embeddings:search', toolbar.$.mainToolbar.searchIconOverride);
    toolbar.selectedPage = 'grouped';
    assertEquals(
        'history-embeddings:search', toolbar.$.mainToolbar.searchIconOverride);

    // Synced tabs page should have the default icon.
    toolbar.selectedPage = 'syncedTabs';
    assertEquals(undefined, toolbar.$.mainToolbar.searchIconOverride);
  });

  test('updates search input aria-description', async () => {
    // Without history embeddings enabled, description should be empty.
    loadTimeData.overrideValues({enableHistoryEmbeddings: false});
    let toolbar = createToolbar();
    await flushTasks();
    toolbar.selectedPage = 'history';
    assertEquals('', toolbar.$.mainToolbar.searchInputAriaDescription);

    // With history embeddings enabled, description should change.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      historyEmbeddingsDisclaimer: 'some disclaimer',
    });
    toolbar = createToolbar();
    await flushTasks();
    toolbar.selectedPage = 'history';
    assertEquals(
        'some disclaimer', toolbar.$.mainToolbar.searchInputAriaDescription);
    toolbar.selectedPage = 'grouped';
    assertEquals(
        'some disclaimer', toolbar.$.mainToolbar.searchInputAriaDescription);

    // Synced tabs page should have no description.
    toolbar.selectedPage = 'syncedTabs';
    assertEquals(undefined, toolbar.$.mainToolbar.searchInputAriaDescription);
  });

  test('updates search input prompt', async () => {
    // Without history embeddings enabled, prompt should be default.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: false,
      searchPrompt: 'Search history',
    });
    let toolbar = createToolbar();
    await flushTasks();
    toolbar.selectedPage = 'history';
    assertEquals('Search history', toolbar.$.mainToolbar.searchPrompt);

    // With history embeddings enabled, prompt should change.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      historyEmbeddingsSearchPrompt: 'Describe your search',
    });
    toolbar = createToolbar();
    await flushTasks();
    toolbar.selectedPage = 'history';
    assertEquals('Describe your search', toolbar.$.mainToolbar.searchPrompt);

    // Synced tabs page should have the default prompt.
    toolbar.selectedPage = 'syncedTabs';
    assertEquals('Search history', toolbar.$.mainToolbar.searchPrompt);
  });
});
