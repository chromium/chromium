// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';

import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsElement} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import type {SearchResultItem} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings', () => {
  let element: HistoryEmbeddingsElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  const mockResults: SearchResultItem[] = [
    {
      title: 'Google',
      url: {url: 'http://google.com'},
      urlForDisplay: 'google.com',
      relativeTime: '2 hours ago',
      sourcePassage: 'Google description',
    },
    {
      title: 'Youtube',
      url: {url: 'http://youtube.com'},
      urlForDisplay: 'youtube.com',
      relativeTime: '4 hours ago',
      sourcePassage: 'Youtube description',
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(PageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(handler));
    handler.setResultFor(
        'search', Promise.resolve({result: {items: mockResults}}));

    element = document.createElement('cr-history-embeddings');
    document.body.appendChild(element);

    element.searchQuery = 'some query';
    await handler.whenCalled('search');
    return flushTasks();
  });

  test('Searches', async () => {
    assertEquals('some query', handler.getArgs('search')[0].query);
  });

  test('DisplaysHeading', async () => {
    loadTimeData.overrideValues({
      historyEmbeddingsHeading: 'searched for "$1"',
      historyEmbeddingsHeadingLoading: 'loading results for "$1"',
    });
    element.searchQuery = 'my query';
    assertEquals(
        'loading results for "my query"',
        element.$.heading.textContent!.trim());
    await handler.whenCalled('search');
    await flushTasks();
    assertEquals(
        'searched for "my query"', element.$.heading.textContent!.trim());
  });

  test('DisplaysResults', async () => {
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    assertEquals(2, resultsElements.length);
    assertEquals('Google', resultsElements[0]!.title);
    assertEquals('Youtube', resultsElements[1]!.title);
  });

  test('FiresClick', async () => {
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    const resultClickEventPromise = eventToPromise('result-click', element);
    resultsElements[0]!.click();
    const resultClickEvent = await resultClickEventPromise;
    assertEquals(mockResults[0], resultClickEvent.detail);
  });

  test('FiresClickOnMoreActions', async () => {
    const moreActionsIconButtons =
        element.shadowRoot!.querySelectorAll<HTMLElement>(
            'cr-url-list-item cr-icon-button');
    const moreActionsClickEventPromise =
        eventToPromise('more-actions-click', element);
    moreActionsIconButtons[0]!.click();
    const moreActionsClickEvent = await moreActionsClickEventPromise;
    assertEquals(mockResults[0], moreActionsClickEvent.detail);
  });
});
