// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';

import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsElement} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('cr-history-embeddings', () => {
  let element: HistoryEmbeddingsElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(PageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(handler));
    handler.setResultFor('doSomething', Promise.resolve(true));

    element = document.createElement('cr-history-embeddings');
    document.body.appendChild(element);
    return flushTasks();
  });

  test('CallsProxy', async () => {
    await handler.whenCalled('doSomething');
    assertEquals(1, handler.getCallCount('doSomething'));
  });
});
