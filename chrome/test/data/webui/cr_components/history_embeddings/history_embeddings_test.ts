// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';

import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsElement} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings', () => {
  let element: HistoryEmbeddingsElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(PageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(handler));
    handler.setResultFor('doSomething', Promise.resolve(true));
    handler.setResultFor('search', Promise.resolve({items: []}));

    element = document.createElement('cr-history-embeddings');
    document.body.appendChild(element);
    return flushTasks();
  });

  test('CallsProxy', async () => {
    await handler.whenCalled('doSomething');
    assertEquals(1, handler.getCallCount('doSomething'));
    await handler.whenCalled('search');
    assertEquals(1, handler.getCallCount('search'));
  });

  test('DisplaysHeading', () => {
    loadTimeData.overrideValues(
        {historyEmbeddingsHeading: 'searched for "$1"'});
    element.searchQuery = 'my query';
    assertEquals(
        'searched for "my query"', element.$.heading.textContent!.trim());
  });

  test('DisplaysResults', async () => {
    element.results = [
      {domain: 'google.com', title: 'Google', url: 'http://google.com'},
      {domain: 'youtube.com', title: 'Youtube', url: 'http://youtube.com'},
    ];
    await flushTasks();
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    assertEquals(2, resultsElements.length);
    assertEquals('Google', resultsElements[0]!.title);
    assertEquals('Youtube', resultsElements[1]!.title);
  });

  test('FiresClick', async () => {
    element.results = [
      {domain: 'google.com', title: 'Google', url: 'http://google.com'},
      {domain: 'youtube.com', title: 'Youtube', url: 'http://youtube.com'},
    ];
    await flushTasks();
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    const resultClickEventPromise = eventToPromise('result-click', element);
    resultsElements[0]!.click();
    const resultClickEvent = await resultClickEventPromise;
    assertEquals(element.results[0], resultClickEvent.detail);
  });
});
