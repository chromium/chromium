// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';

import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import {HistoryClustersElement} from 'chrome://resources/cr_components/history_clusters/clusters.js';
import type {Cluster, RawVisitData, URLVisit} from 'chrome://resources/cr_components/history_clusters/history_cluster_types.mojom-webui.js';
import type {PageRemote, QueryResult} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {ClientId as PageImageServiceClientId, PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
let callbackRouterRemote: PageRemote;
let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
    PageImageServiceHandlerRemote;

function createBrowserProxy() {
  handler = TestMock.fromClass(PageHandlerRemote);
  const callbackRouter = new PageCallbackRouter();
  BrowserProxyImpl.setInstance(new BrowserProxyImpl(handler, callbackRouter));
  callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

  imageServiceHandler = TestMock.fromClass(PageImageServiceHandlerRemote);
  PageImageServiceBrowserProxy.setInstance(
      new PageImageServiceBrowserProxy(imageServiceHandler));
}

suite('history-clusters', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues({
      isHistoryClustersImagesEnabled: true,
    });
  });

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
      debugInfo: {},
      rawVisitData: rawVisitData,
      isKnownToSync: false,
      hasUrlKeyedImage: false,
    };

    const cluster1: Cluster = {
      id: BigInt(111),
      visits: [urlVisit1],
      label: '',
      labelMatchPositions: [],
      relatedSearches: [],
      imageUrl: null,
      fromPersistence: false,
      debugInfo: null,
      tabGroupName: null,
    };

    const cluster2: Cluster = {
      id: BigInt(222),
      visits: [],
      label: '',
      labelMatchPositions: [],
      relatedSearches: [],
      imageUrl: null,
      fromPersistence: false,
      debugInfo: null,
      tabGroupName: null,
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

  test('Updates IsEmpty attribute', async () => {
    const clustersElement = new HistoryClustersElement();
    document.body.appendChild(clustersElement);
    await handler.whenCalled('startQueryClusters');

    callbackRouterRemote.onClustersQueryResult({
      query: '',
      clusters: [],
      canLoadMore: false,
      isContinuation: false,
    });
    await callbackRouterRemote.$.flushForTesting();
    await flushTasks();

    assertTrue(clustersElement.isEmpty);
  });

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

  test('url visit requests image', async () => {
    const clustersElement = await setupClustersElement();

    callbackRouterRemote.onClustersQueryResult(getTestResult());
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    // Set a result for the image handler to pass back to the favicon component,
    // so it doesn't throw a console error.
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: {imageUrl: {url: 'https://example.com/image.png'}},
    }));

    const cluster = clustersElement.$.clusters.querySelector('history-cluster');
    assertTrue(!!cluster);
    const urlVisit = cluster.$.container.querySelector('url-visit');
    assertTrue(!!urlVisit);
    // Assign a copied visit object with `isKnownToSync` set to true.
    const copiedVisit =
        Object.assign({}, urlVisit.visit, {isKnownToSync: true});
    const copiedCluster = Object.assign({}, cluster.cluster);
    copiedCluster.visits[0] = copiedVisit;
    cluster.cluster = copiedCluster;

    const [clientId, pageUrl] =
        await imageServiceHandler.whenCalled('getPageImageUrl');
    await microtasksFinished();
    assertEquals(PageImageServiceClientId.Journeys, clientId);
    assertTrue(!!urlVisit.visit);
    assertEquals(urlVisit.visit.normalizedUrl, pageUrl);

    // Verify the icon element received the handler's response.
    const icon = urlVisit.shadowRoot!.querySelector('page-favicon');
    assertTrue(!!icon);
    const imageUrl = icon.getImageUrlForTesting();
    assertTrue(!!imageUrl);
    assertEquals('https://example.com/image.png', imageUrl.url);

    // Verify that the icon's image can be cleared.
    imageServiceHandler.reset();
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: null,
    }));
    icon.url = {url: 'https://something-different.com'};
    const [newClientId, newPageUrl] =
        await imageServiceHandler.whenCalled('getPageImageUrl');
    await microtasksFinished();
    assertEquals(PageImageServiceClientId.Journeys, newClientId);
    assertTrue(!!newPageUrl);
    assertEquals('https://something-different.com', newPageUrl.url);
    assertTrue(!icon.getImageUrlForTesting());
  });

  test('sets scroll target', async () => {
    const clustersElement = await setupClustersElement();
    clustersElement.scrollTarget = document.body;
    await microtasksFinished();

    assertEquals(document.body, clustersElement.$.clusters.scrollTarget);
  });

  test('sets scroll offset', async () => {
    const clustersElement = await setupClustersElement();
    clustersElement.scrollOffset = 123;
    await microtasksFinished();
    assertEquals(123, clustersElement.$.clusters.scrollOffset);
  });

  test('loads more results for tall monitors', async () => {
    const clustersElement = new HistoryClustersElement();
    clustersElement.scrollTarget = document.body;
    document.body.appendChild(clustersElement);
    await handler.whenCalled('startQueryClusters');
    handler.reset();

    // `canLoadMore` set to false should not load more results.
    callbackRouterRemote.onClustersQueryResult(
        Object.assign(getTestResult(), {canLoadMore: false}));
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(
        0, handler.getCallCount('loadMoreClusters'),
        'should not load more results');

    // Make scroll target very short. Even if `canLoadMore` is set to true,
    // more results should not be loaded since the scroll target is already
    // filled.
    document.body.style.height = '2px';
    callbackRouterRemote.onClustersQueryResult(
        Object.assign(getTestResult(), {canLoadMore: true}));
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(
        0, handler.getCallCount('loadMoreClusters'),
        'should not load more results for short scroll target');

    // Make scroll target very tall. Now, more results should be loaded since
    // the scroll target has plenty of extra unfilled space.
    document.body.style.height = '2000px';
    callbackRouterRemote.onClustersQueryResult(
        Object.assign(getTestResult(), {canLoadMore: true}));
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(
        1, handler.getCallCount('loadMoreClusters'),
        'should load more results for tall scroll target');
  });
});
