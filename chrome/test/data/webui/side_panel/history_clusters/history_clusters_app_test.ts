// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';

import type {HistoryClustersAppElement} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
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

  test('SwitchesScrollContainer', async () => {
    // When embeddings is enabled, scroll target should be the
    // embeddingsScrollContainer and the container should have styles on it.
    assertEquals(
        'flex', getComputedStyle(app.$.embeddingsScrollContainer).display);
    assertEquals(
        app.$.embeddingsScrollContainer, app.$.historyClusters.scrollTarget);
    assertEquals(
        'sp-scroller sp-scroller-bottom-of-page',
        app.$.embeddingsScrollContainer.className);
    assertEquals('', app.$.historyClusters.className);

    // Create a new app after force disabling embeddings.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({enableHistoryEmbeddings: false});
    app = document.createElement('history-clusters-app');
    document.body.appendChild(app);
    await microtasksFinished();

    // When embeddings are disabled, scroll target should be the
    // history-clusters element.
    assertEquals(
        'contents', getComputedStyle(app.$.embeddingsScrollContainer).display);
    assertEquals(app.$.historyClusters, app.$.historyClusters.scrollTarget);
    assertEquals(
        'sp-scroller sp-scroller-bottom-of-page',
        app.$.historyClusters.className);
  });
});
