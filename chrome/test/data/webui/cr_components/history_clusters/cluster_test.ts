// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_clusters/cluster.js';

import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import type {ClusterElement} from 'chrome://resources/cr_components/history_clusters/cluster.js';
import type {Cluster, RawVisitData, URLVisit} from 'chrome://resources/cr_components/history_clusters/history_cluster_types.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

function createBrowserProxy() {
  handler = TestMock.fromClass(PageHandlerRemote);
  const callbackRouter = new PageCallbackRouter();
  BrowserProxyImpl.setInstance(new BrowserProxyImpl(handler, callbackRouter));
  callbackRouter.$.bindNewPipeAndPassRemote();
}

function getTestCluster(): Cluster {
  const rawVisitData: RawVisitData = {
    url: {url: ''},
    visitTime: {internalValue: BigInt(0)},
  };

  const urlVisit1: URLVisit = {
    visitId: BigInt(1),
    normalizedUrl: {url: 'https://www.google.com'},
    urlForDisplay: 'https://www.google.com',
    pageTitle: '',
    titleMatchPositions: [],
    urlForDisplayMatchPositions: [],
    duplicates: [],
    relativeDate: '',
    annotations: [],
    debugInfo: {},
    rawVisitData: rawVisitData,
    isKnownToSync: false,
    hasUrlKeyedImage: false,
  };

  const urlVisit2: URLVisit = {
    visitId: BigInt(2),
    normalizedUrl: {url: 'https://www.example.com'},
    urlForDisplay: 'https://www.example.com',
    pageTitle: '',
    titleMatchPositions: [],
    urlForDisplayMatchPositions: [],
    duplicates: [],
    relativeDate: '',
    annotations: [],
    debugInfo: {},
    rawVisitData: rawVisitData,
    isKnownToSync: false,
    hasUrlKeyedImage: false,
  };

  const relatedSearch1 = {
    query: 'abc',
    url: {url: 'https://www.google.com'},
  };
  const relatedSearch2 = {
    query: 'example',
    url: {url: 'https://www.example.com'},
  };

  return {
    id: BigInt(111),
    visits: [urlVisit1, urlVisit2],
    label: '',
    labelMatchPositions: [],
    relatedSearches: [relatedSearch1, relatedSearch2],
    imageUrl: null,
    fromPersistence: false,
    debugInfo: null,
    tabGroupName: null,
  };
}

suite('cluster element', () => {
  let cluster: ClusterElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isHistoryClustersImagesEnabled: true,
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createBrowserProxy();
    cluster = document.createElement('history-cluster');
    cluster.cluster = getTestCluster();
    cluster.index = 0;
    document.body.appendChild(cluster);
  });

  test('Fires events for url visit actions', async () => {
    const menu = cluster.shadowRoot!.querySelector('cluster-menu');
    assertTrue(!!menu);
    // Open cluster action menu.
    menu.$.actionMenuButton.click();
    await microtasksFinished();

    const hideEvent = eventToPromise('hide-visits', cluster);
    const hideAllButton =
        menu.shadowRoot!.querySelector<HTMLButtonElement>('#hideAllButton');
    assertTrue(!!hideAllButton);
    hideAllButton.click();
    await hideEvent;

    await microtasksFinished();
    // Re-open cluster action menu.
    menu.$.actionMenuButton.click();
    await microtasksFinished();
    const removeAllButton =
        menu.shadowRoot!.querySelector<HTMLButtonElement>('#removeAllButton');
    assertTrue(!!removeAllButton);
    const removeEvent = eventToPromise('remove-visits', cluster);
    removeAllButton.click();
    await removeEvent;

    await microtasksFinished();
    // Re-open cluster action menu.
    menu.$.actionMenuButton.click();
    await microtasksFinished();
    const openAllButton =
        menu.shadowRoot!.querySelector<HTMLButtonElement>('#openAllButton');
    assertTrue(!!openAllButton);
    openAllButton.click();
    await handler.whenCalled('openVisitUrlsInTabGroup');
  });

  test('displays search queries', async () => {
    const queries = cluster.shadowRoot!.querySelectorAll('search-query');
    assertEquals(2, queries.length);

    queries[0]!.$.searchQueryLink.click();
    const params = await handler.whenCalled('openHistoryCluster');
    assertEquals('https://www.google.com', params[0].url);
  });
});
