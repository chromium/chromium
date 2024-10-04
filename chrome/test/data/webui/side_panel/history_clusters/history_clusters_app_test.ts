// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history-clusters-side-panel.top-chrome/app.js';

import type {HistoryClustersAppElement} from 'chrome://history-clusters-side-panel.top-chrome/app.js';
import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import {PageHandlerRemote as HistoryEmbeddingsPageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HistoryClustersAppWithEmbeddingsTest', () => {
  let app: HistoryClustersAppElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(
            TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote)));

    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      historyEmbeddingsSearchMinimumWordCount: 2,
    });

    app = document.createElement('history-clusters-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  function getEmbeddingsComponent() {
    return app.shadowRoot!.querySelector('cr-history-embeddings');
  }

  test('SwitchesSearchIcon', async () => {
    assertEquals('history-embeddings:search', app.$.searchbox.iconOverride);

    // Disable history embeddings and verify icon has switched.
    loadTimeData.overrideValues({enableHistoryEmbeddings: false});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('history-clusters-app');
    document.body.appendChild(app);
    await microtasksFinished();
    assertEquals('', app.$.searchbox.iconOverride);
  });

  test('ShowsResultsComponent', async () => {
    app.query = 'onlyonewordquery';
    await app.updateComplete;
    let embeddingsComponent = getEmbeddingsComponent();
    assertFalse(!!embeddingsComponent);

    app.query = 'two words';
    await app.updateComplete;
    embeddingsComponent = getEmbeddingsComponent();
    assertTrue(!!embeddingsComponent);
  });
});
