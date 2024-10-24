// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';

import type {HistoryClustersAppElement} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {BrowserProxyImpl, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote, PageCallbackRouter, PageHandlerRemote} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HistoryClustersAppWithEmbeddingsTest', () => {
  let app: HistoryClustersAppElement;
  let clustersHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    clustersHandler = TestMock.fromClass(PageHandlerRemote);
    const callbackRouter = new PageCallbackRouter();
    BrowserProxyImpl.setInstance(
        new BrowserProxyImpl(clustersHandler, callbackRouter));

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

  async function forceEmbeddingsComponent() {
    // Force a search so that the cr-history-embeddings component is available.
    app.query = 'two words';
    await app.updateComplete;
    const embeddingsComponent = getEmbeddingsComponent();
    assertTrue(!!embeddingsComponent);
    return embeddingsComponent;
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
    const historyEmbeddingsElement = await forceEmbeddingsComponent();
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

  test('OpensEmbeddingItems', async () => {
    const embeddingsComponent = await forceEmbeddingsComponent();
    const mockItem = {
      title: 'Google',
      url: {url: 'http://google.com'},
      urlForDisplay: 'google.com',
      relativeTime: '2 hours ago',
      shortDateTime: 'Sept 2, 2022',
      sourcePassage: 'Google description',
      lastUrlVisitTimestamp: 1000,
      answerData: null,
    };
    const mouseEventArgs = {
      middleButton: true,
      altKey: true,
      ctrlKey: false,
      metaKey: false,
      shiftKey: true,
    };
    embeddingsComponent.dispatchEvent(new CustomEvent('result-click', {
      detail: {
        item: mockItem,
        ...mouseEventArgs,
      },
    }));
    const openUrlArgs = await clustersHandler.whenCalled('openHistoryUrl');
    assertDeepEquals(mockItem.url, openUrlArgs[0]);
    assertDeepEquals(mouseEventArgs, openUrlArgs[1]);
  });

  test('ShowsContextMenusForEmbeddingItems', async () => {
    const embeddingsComponent = await forceEmbeddingsComponent();
    const mockItem = {
      title: 'Google',
      url: {url: 'http://google.com'},
      urlForDisplay: 'google.com',
      relativeTime: '2 hours ago',
      shortDateTime: 'Sept 2, 2022',
      sourcePassage: 'Google description',
      lastUrlVisitTimestamp: 1000,
      answerData: null,
    };
    const mouseEventArgs = {x: 100, y: 200};
    embeddingsComponent.dispatchEvent(new CustomEvent('result-context-menu', {
      detail: {
        item: mockItem,
        ...mouseEventArgs,
      },
    }));
    const contextMenuArgs =
        await clustersHandler.whenCalled('showContextMenuForURL');
    assertDeepEquals(mockItem.url, contextMenuArgs[0]);
    assertDeepEquals(mouseEventArgs, contextMenuArgs[1]);
  });

  test('RemovesEmbeddingItems', async () => {
    const embeddingsComponent = await forceEmbeddingsComponent();
    assertTrue(!!embeddingsComponent);
    embeddingsComponent.dispatchEvent(new CustomEvent('remove-item-click', {
      detail: {
        title: 'Google',
        url: {url: 'http://google.com'},
        urlForDisplay: 'google.com',
        relativeTime: '2 hours ago',
        sourcePassage: 'Google description',
        lastUrlVisitTimestamp: 1000,
      },
    }));
    const removeVisitArgs =
        await clustersHandler.whenCalled('removeVisitByUrlAndTime');
    assertDeepEquals({url: 'http://google.com'}, removeVisitArgs[0]);
    assertEquals(1000, removeVisitArgs[1]);
  });

  test('SendsClusterClickToEmbeddings', async () => {
    const embeddingsComponent = await forceEmbeddingsComponent();
    assertTrue(!!embeddingsComponent);
    assertFalse(embeddingsComponent.otherHistoryResultClicked);

    app.$.historyClusters.dispatchEvent(
        new CustomEvent('record-history-link-click'));
    await app.updateComplete;
    assertTrue(embeddingsComponent.otherHistoryResultClicked);

    app.$.searchbox.dispatchEvent(
        new CustomEvent('search-changed', {detail: 'new query'}));
    await app.updateComplete;
    assertFalse(embeddingsComponent.otherHistoryResultClicked);
  });

  test('CountsCharacters', async () => {
    const embeddingsComponent = await forceEmbeddingsComponent();
    assertTrue(!!embeddingsComponent);
    assertEquals(0, embeddingsComponent.numCharsForQuery);

    function dispatchNativeInput(
        inputEvent: Partial<InputEvent>, inputValue: string) {
      app.$.searchbox.dispatchEvent(
          new CustomEvent('search-term-native-input', {
            detail: {e: inputEvent, inputValue},
            composed: true,
            bubbles: true,
          }));
    }

    dispatchNativeInput({data: 'a'}, 'a');
    await app.updateComplete;
    assertEquals(
        1, embeddingsComponent.numCharsForQuery, 'counts normal characters');
    dispatchNativeInput({data: 'b'}, 'ab');
    dispatchNativeInput({data: 'c'}, 'abc');
    await app.updateComplete;
    assertEquals(
        3, embeddingsComponent.numCharsForQuery,
        'counts additional characters');

    dispatchNativeInput({data: 'pasted text'}, 'pasted text');
    await app.updateComplete;
    assertEquals(
        1, embeddingsComponent.numCharsForQuery,
        'insert that replaces all text counts as 1');

    dispatchNativeInput({data: 'more text'}, 'pasted text more text');
    await app.updateComplete;
    assertEquals(
        2, embeddingsComponent.numCharsForQuery,
        'insert that adds to existing input increments count');

    dispatchNativeInput({data: null}, 'pasted text more tex');
    await app.updateComplete;
    assertEquals(
        3, embeddingsComponent.numCharsForQuery, 'deletion increments');

    dispatchNativeInput({data: null}, '');
    await app.updateComplete;
    assertEquals(
        0, embeddingsComponent.numCharsForQuery,
        'deletion of entire input resets counter');

    app.$.searchbox.dispatchEvent(new CustomEvent('search-term-cleared'));
    await app.updateComplete;
    assertEquals(0, embeddingsComponent.numCharsForQuery, 'resets on clear');
  });

  test('HidesEmptyClusters', async () => {
    const clustersComponent = app.$.historyClusters;
    assertTrue(!!clustersComponent);
    assertTrue(isVisible(clustersComponent));

    // Pretend that history-clusters is empty.
    clustersComponent.isEmpty = true;
    await app.updateComplete;
    assertTrue(isVisible(clustersComponent));

    // When history-embeddings has results, history-clusters should be hidden.
    const embeddingsComponent = await forceEmbeddingsComponent();
    embeddingsComponent.dispatchEvent(
        new CustomEvent('is-empty-changed', {detail: {value: false}}));
    await app.updateComplete;
    assertFalse(isVisible(clustersComponent));

    // When history-embeddings is empty, history-clusters should be visible
    // again.
    embeddingsComponent.dispatchEvent(
        new CustomEvent('is-empty-changed', {detail: {value: true}}));
    await app.updateComplete;
    assertTrue(isVisible(clustersComponent));
  });
});
