// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';

import type {HistoryClustersAppElement} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HistoryClustersAppWithEmbeddingsTest', () => {
  let app: HistoryClustersAppElement;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    embeddingsHandler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(embeddingsHandler));

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

  test('DisclaimerLink', async () => {
    // Force a search so that the cr-history-embeddings component is available.
    app.query = 'two words';
    await app.updateComplete;

    const historyEmbeddingsElement =
        app.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.forceSuppressLogging);

    const disclaimerLink = app.shadowRoot!.querySelector<HTMLElement>(
        '#historyEmbeddingsDisclaimerLink');
    assertTrue(!!disclaimerLink);
    assertTrue(isVisible(disclaimerLink));

    const clickEvent = new Event('click', {cancelable: true});
    disclaimerLink.dispatchEvent(clickEvent);
    await embeddingsHandler.whenCalled('openSettingsPage');
    assertTrue(clickEvent.defaultPrevented);
    assertEquals(1, embeddingsHandler.getCallCount('openSettingsPage'));

    await app.updateComplete;
    assertTrue(historyEmbeddingsElement.forceSuppressLogging);
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
