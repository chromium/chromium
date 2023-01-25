// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import {HistoryClustersElement} from 'chrome://resources/cr_components/history_clusters/clusters.js';
import {Cluster, PageCallbackRouter, PageHandlerRemote, PageRemote, QueryResult, RawVisitData, URLVisit} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

let handler: PageHandlerRemote&TestBrowserProxy;
let callbackRouterRemote: PageRemote;

function createBrowserProxy() {
  handler = TestBrowserProxy.fromClass(PageHandlerRemote);
  const callbackRouter = new PageCallbackRouter();
  BrowserProxyImpl.setInstance(new BrowserProxyImpl(handler, callbackRouter));
  callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
}

suite('history-clusters', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    createBrowserProxy();
  });

  function getTestResult(): QueryResult {
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
      hidden: false,
      debugInfo: {},
      rawVisitData: rawVisitData,
      isKnownToSync: false,
    };

    const cluster1: Cluster = {
      id: BigInt(111),
      visits: [urlVisit1],
      labelMatchPositions: [],
      relatedSearches: [],
      fromPersistence: false,
    };

    const cluster2: Cluster = {
      id: BigInt(222),
      visits: [],
      labelMatchPositions: [],
      relatedSearches: [],
      fromPersistence: false,
    };

    const queryResult: QueryResult = {
      query: '',
      clusters: [cluster1, cluster2],
      canLoadMore: false,
      isContinuation: false,
    };

    return queryResult;
  }

  async function setupClustersElement() {
    const clustersElement = new HistoryClustersElement();
    document.body.appendChild(clustersElement);

    const query = (await handler.whenCalled('startQueryClusters'))[0];
    assertEquals(query, '');

    callbackRouterRemote.onClustersQueryResult(getTestResult());
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    // Make the handler ready for new assertions.
    handler.reset();

    return clustersElement;
  }

  test('List displays one element per cluster', async () => {
    const clustersElement = await setupClustersElement();

    const ironListItems = clustersElement.$.clusters.items!;
    assertEquals(ironListItems.length, 2);
  });

  test('Externally deleted history triggers re-query', async () => {
    // We don't directly reference the clusters element here.
    await setupClustersElement();

    callbackRouterRemote.onHistoryDeleted();
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    const newQuery = (await handler.whenCalled('startQueryClusters'))[0];
    assertEquals(newQuery, '');
  });

  test('Non-empty query', async () => {
    const clustersElement = await setupClustersElement();
    clustersElement.query = 'foobar';

    const query = (await handler.whenCalled('startQueryClusters'))[0];
    assertEquals(query, 'foobar');

    callbackRouterRemote.onClustersQueryResult(getTestResult());
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    // When History is externally deleted, we should hit the backend with the
    // same query again.
    handler.reset();
    callbackRouterRemote.onHistoryDeleted();
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    const newQuery = (await handler.whenCalled('startQueryClusters'))[0];
    assertEquals(newQuery, 'foobar');
  });

  test('Navigate to url visit via click', async () => {
    const clustersElement = await setupClustersElement();

    callbackRouterRemote.onClustersQueryResult(getTestResult());
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    const urlVisit =
        clustersElement.$.clusters.querySelector('history-cluster')!.$.container
            .querySelector('url-visit');
    const urlVisitHeader =
        urlVisit!.shadowRoot!.querySelector<HTMLElement>('#header');

    urlVisitHeader!.click();

    const openHistoryClusterArgs =
        await handler.whenCalled('openHistoryCluster');

    assertEquals(urlVisit!.$.url.innerHTML, openHistoryClusterArgs[0].url);
    assertEquals(1, handler.getCallCount('openHistoryCluster'));
  });

  test('Navigate to url visit via keyboard', async () => {
    const clustersElement = await setupClustersElement();

    callbackRouterRemote.onClustersQueryResult(getTestResult());
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    const urlVisit =
        clustersElement.$.clusters.querySelector('history-cluster')!.$.container
            .querySelector('url-visit');
    const urlVisitHeader =
        urlVisit!.shadowRoot!.querySelector<HTMLElement>('#header');

    // First url visit is selected.
    urlVisitHeader!.focus();

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    urlVisitHeader!.dispatchEvent(shiftEnter);

    // Navigates to the first match is selected.
    const openHistoryClusterArgs =
        await handler.whenCalled('openHistoryCluster');

    assertEquals(urlVisit!.$.url.innerHTML, openHistoryClusterArgs[0].url);
    assertEquals(true, openHistoryClusterArgs[1].shiftKey);
    assertEquals(1, handler.getCallCount('openHistoryCluster'));
  });
});
