// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import {HistoryClustersElement} from 'chrome://resources/cr_components/history_clusters/clusters.js';
import {Cluster, RawVisitData, URLVisit} from 'chrome://resources/cr_components/history_clusters/history_cluster_types.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote, PageRemote, QueryResult} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {ImageServiceBrowserProxy} from 'chrome://resources/cr_components/image_service/browser_proxy.js';
import {ClientId as ImageServiceClientId, ImageServiceHandlerRemote} from 'chrome://resources/cr_components/image_service/image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
let callbackRouterRemote: PageRemote;
let imageServiceHandler: TestMock<ImageServiceHandlerRemote>&
    ImageServiceHandlerRemote;

function createBrowserProxy() {
  handler = TestMock.fromClass(PageHandlerRemote);
  const callbackRouter = new PageCallbackRouter();
  BrowserProxyImpl.setInstance(new BrowserProxyImpl(handler, callbackRouter));
  callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

  imageServiceHandler = TestMock.fromClass(ImageServiceHandlerRemote);
  ImageServiceBrowserProxy.setInstance(
      new ImageServiceBrowserProxy(imageServiceHandler));
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
      imageUrl: undefined,
    };

    const cluster1: Cluster = {
      id: BigInt(111),
      visits: [urlVisit1],
      label: undefined,
      labelMatchPositions: [],
      relatedSearches: [],
      imageUrl: undefined,
      fromPersistence: false,
      debugInfo: undefined,
    };

    const cluster2: Cluster = {
      id: BigInt(222),
      visits: [],
      label: undefined,
      labelMatchPositions: [],
      relatedSearches: [],
      imageUrl: undefined,
      fromPersistence: false,
      debugInfo: undefined,
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

    const urlVisit =
        clustersElement.$.clusters.querySelector('history-cluster')!.$.container
            .querySelector('url-visit');
    assertTrue(!!urlVisit);
    // Assign a copied visit object with `isKnownToSync` set to true.
    urlVisit.visit = Object.assign({}, urlVisit.visit, {isKnownToSync: true});

    const [clientId, pageUrl] =
        await imageServiceHandler.whenCalled('getPageImageUrl');
    assertEquals(ImageServiceClientId.Journeys, clientId);
    assertEquals(urlVisit.visit.normalizedUrl, pageUrl);

    // Verify the icon element received the handler's response.
    const icon = urlVisit.shadowRoot!.querySelector('page-favicon');
    assertTrue(!!icon);
    const imageUrl = icon.getImageUrlForTesting();
    assertTrue(!!imageUrl);
    assertEquals('https://example.com/image.png', imageUrl.url);
  });
});