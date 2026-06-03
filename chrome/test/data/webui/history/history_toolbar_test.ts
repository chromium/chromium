// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserProxyImpl, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history/history.js';
import type {HistoryEntry, QueryResult} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryEntry, createHistoryInfo} from './test_util.js';

suite('history-toolbar', function() {
  let app: HistoryAppElement;
  let testProxy: TestHistoryBrowserProxy;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;
  const TEST_HISTORY_RESULTS: [HistoryEntry] =
      [createHistoryEntry('2016-03-15', 'https://google.com')];

  function createToolbar() {
    const toolbar = document.createElement('history-toolbar');
    document.body.appendChild(toolbar);
    return toolbar;
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);
    embeddingsHandler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(embeddingsHandler));
    embeddingsHandler.setResultFor(
        'search', Promise.resolve({result: {items: []}}));

    app = document.createElement('history-app');
    document.body.appendChild(app);
    await testProxy.handler.whenCalled('queryHistory');
    return microtasksFinished();
  });

  test('selecting checkbox causes toolbar to change', async function() {
    testProxy.handler.setResultFor('queryHistoryContinuation', Promise.resolve({
      results: {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS},
    }));
    app.$.history.dispatchEvent(new CustomEvent(
        'query-history', {bubbles: true, composed: true, detail: true}));
    await testProxy.handler.whenCalled('queryHistoryContinuation');
    await microtasksFinished();
    const item = app.$.history.shadowRoot.querySelector('history-item')!;
    item.$.checkbox.click();
    await microtasksFinished();

    const toolbar = app.$.toolbar;

    // Ensure that when an item is selected that the count held by the
    // toolbar increases.
    assertEquals(1, toolbar.count);
    assertTrue(toolbar.$.mainToolbar.hasAttribute('has-overlay'));

    item.$.checkbox.click();
    await item.$.checkbox.updateComplete;
    await microtasksFinished();

    // Ensure that when an item is deselected the count held by the
    // toolbar decreases.
    assertEquals(0, toolbar.count);
    assertFalse(toolbar.$.mainToolbar.hasAttribute('has-overlay'));
  });

  test('search term gathered correctly from toolbar', async function() {
    testProxy.handler.resetResolver('queryHistory');
    const toolbar = app.$.toolbar;
    testProxy.handler.setResultFor('queryHistory', Promise.resolve({
      results: {info: createHistoryInfo('Test'), value: TEST_HISTORY_RESULTS},
    }));
    toolbar.$.mainToolbar.dispatchEvent(new CustomEvent(
        'search-changed', {bubbles: true, composed: true, detail: 'Test'}));
    const queryArgs = await testProxy.handler.whenCalled('queryHistory');
    assertEquals('Test', queryArgs[0]);
  });

  test('spinner is active on search', async function() {
    testProxy.handler.resetResolver('queryHistory');

    const delayedQuery = new PromiseResolver<{results: QueryResult}>();

    testProxy.handler.setResultFor('queryHistory', delayedQuery.promise);

    const toolbar = app.$.toolbar;
    toolbar.$.mainToolbar.dispatchEvent(new CustomEvent(
        'search-changed', {bubbles: true, composed: true, detail: 'Test2'}));
    await testProxy.handler.whenCalled('queryHistory');
    await microtasksFinished();

    assertTrue(toolbar.spinnerActive);
    delayedQuery.resolve({
      results: {
        info: createHistoryInfo('Test2'),
        value: TEST_HISTORY_RESULTS,
      },
    });
    await microtasksFinished();
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
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertEquals('', toolbar.$.mainToolbar.searchIconOverride);

    // With history embeddings enabled, search icon should change.
    loadTimeData.overrideValues({enableHistoryEmbeddings: true});
    toolbar = createToolbar();
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertEquals(
        'history-embeddings:search', toolbar.$.mainToolbar.searchIconOverride);
    toolbar.selectedPage = 'grouped';
    await microtasksFinished();
    assertEquals(
        'history-embeddings:search', toolbar.$.mainToolbar.searchIconOverride);

    // Synced tabs page should have the default icon.
    toolbar.selectedPage = 'syncedTabs';
    await microtasksFinished();
    assertEquals('', toolbar.$.mainToolbar.searchIconOverride);
  });

  test('updates search input aria-description', async () => {
    // Without history embeddings enabled, description should be empty.
    loadTimeData.overrideValues({enableHistoryEmbeddings: false});
    let toolbar = createToolbar();
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertEquals('', toolbar.$.mainToolbar.searchInputAriaDescription);

    // With history embeddings enabled, description should change.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      historyEmbeddingsDisclaimer: 'some disclaimer',
    });
    toolbar = createToolbar();
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertEquals(
        'some disclaimer', toolbar.$.mainToolbar.searchInputAriaDescription);
    toolbar.selectedPage = 'grouped';
    await microtasksFinished();
    assertEquals(
        'some disclaimer', toolbar.$.mainToolbar.searchInputAriaDescription);

    // Synced tabs page should have no description.
    toolbar.selectedPage = 'syncedTabs';
    await microtasksFinished();
    assertEquals('', toolbar.$.mainToolbar.searchInputAriaDescription);
  });

  test('updates search input prompt', async () => {
    // Without history embeddings enabled, prompt should be default.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: false,
      searchPrompt: 'Search history',
    });
    let toolbar = createToolbar();
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertEquals('Search history', toolbar.$.mainToolbar.searchPrompt);

    // With history embeddings enabled, prompt should change.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      historyEmbeddingsSearchPrompt: 'Describe your search',
    });
    toolbar = createToolbar();
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertEquals('Describe your search', toolbar.$.mainToolbar.searchPrompt);

    // Synced tabs page should have the default prompt.
    toolbar.selectedPage = 'syncedTabs';
    await microtasksFinished();
    assertEquals('Search history', toolbar.$.mainToolbar.searchPrompt);

    // With history embeddings' answerer enabled, prompt should change.
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      enableHistoryEmbeddingsAnswers: true,
    });
    const possiblePrompts = {
      historyEmbeddingsSearchPrompt: 'prompt 0',
      historyEmbeddingsAnswersSearchAlternativePrompt1: 'prompt 1',
      historyEmbeddingsAnswersSearchAlternativePrompt2: 'prompt 2',
      historyEmbeddingsAnswersSearchAlternativePrompt3: 'prompt 3',
      historyEmbeddingsAnswersSearchAlternativePrompt4: 'prompt 4',
    };
    loadTimeData.overrideValues(possiblePrompts);
    toolbar = createToolbar();
    toolbar.selectedPage = 'history';
    await microtasksFinished();
    assertTrue(Object.values(possiblePrompts)
                   .includes(toolbar.$.mainToolbar.searchPrompt));
  });
});
