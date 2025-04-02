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
import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

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

function getTestVisit(rawData?: RawVisitData): URLVisit {
  const rawVisitData: RawVisitData = rawData || {
    url: {url: ''},
    visitTime: {internalValue: BigInt(0)},
  };

  return {
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
}

function getTestCluster(id: bigint, visits: URLVisit[]) {
  return {
    id: id,
    visits: visits,
    label: '',
    labelMatchPositions: [],
    relatedSearches: [],
    imageUrl: null,
    fromPersistence: false,
    debugInfo: null,
    tabGroupName: null,
  };
}

function getTestResult(): QueryResult {
  const cluster1: Cluster = getTestCluster(BigInt(111), [getTestVisit()]);

  const cluster2: Cluster = getTestCluster(BigInt(222), []);

  const queryResult: QueryResult = {
    query: '',
    clusters: [cluster1, cluster2],
    canLoadMore: false,
    isContinuation: false,
  };

  return queryResult;
}

suite('HistoryClustersTest', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues({
      isHistoryClustersImagesEnabled: true,
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    createBrowserProxy();
  });

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

    const ironListItems = clustersElement.$.clusters.items;
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
        urlVisit!.shadowRoot.querySelector<HTMLElement>('#header');

    urlVisitHeader!.click();

    const openHistoryUrlArgs = await handler.whenCalled('openHistoryUrl');

    assertEquals(urlVisit!.$.url.innerHTML, openHistoryUrlArgs[0].url);
    assertEquals(1, handler.getCallCount('openHistoryUrl'));
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
        urlVisit!.shadowRoot.querySelector<HTMLElement>('#header');

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
    const openHistoryUrlArgs = await handler.whenCalled('openHistoryUrl');

    assertEquals(urlVisit!.$.url.innerHTML, openHistoryUrlArgs[0].url);
    assertEquals(true, openHistoryUrlArgs[1].shiftKey);
    assertEquals(1, handler.getCallCount('openHistoryUrl'));
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
    const icon = urlVisit.shadowRoot.querySelector('page-favicon');
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

  test('loads and renders more results for tall monitors', async () => {
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
    await microtasksFinished();
    // Initial 2 results are rendered.
    assertEquals(
        2,
        clustersElement.shadowRoot.querySelectorAll('history-cluster').length);

    // More clusters requested. Simulate a response.
    callbackRouterRemote.onClustersQueryResult(Object.assign(
        getTestResult(), {canLoadMore: false, isContinuation: true}));
    await microtasksFinished();
    assertEquals(
        4,
        clustersElement.shadowRoot.querySelectorAll('history-cluster').length);
  });

  test('Cluster removed', async () => {
    const clustersElement = await setupClustersElement();
    assertEquals(
        2,
        clustersElement.shadowRoot.querySelectorAll('history-cluster').length);

    callbackRouterRemote.onVisitsRemoved([getTestVisit()]);
    await Promise.all([
      callbackRouterRemote.$.flushForTesting(),
      eventToPromise('remove-cluster', clustersElement),
    ]);
    await microtasksFinished();

    // Cluster 1 is removed since it contained only 1 visit, which matched the
    // removed one.
    assertEquals(
        1,
        clustersElement.shadowRoot.querySelectorAll('history-cluster').length);
  });
});

suite('HistoryClustersFocusTest', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues({
      isHistoryClustersImagesEnabled: true,
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createBrowserProxy();
  });

  // Set up a clusters element that has a fixed size parent and scrollable
  // <div> as the scrollTarget for the list. This simulates a smaller window,
  // so we don't have to implicitly rely on the size of the window in browser
  // tests, which may vary and is not controllable from the test, and allows
  // the element to scroll for any content past a default 300px height.
  async function setupScrollableClustersElement():
      Promise<HistoryClustersElement> {
    document.body.style.height = '300px';
    document.body.style.maxHeight = '300px';
    document.body.style.overflow = 'hidden';

    const scrollTarget = document.createElement('div');
    scrollTarget.style.height = '100%';
    scrollTarget.style.overflowY = 'auto';
    document.body.appendChild(scrollTarget);

    const clustersElement = new HistoryClustersElement();
    clustersElement.scrollTarget = scrollTarget;
    clustersElement.setScrollDebounceForTest(1);
    scrollTarget.appendChild(clustersElement);

    const query = (await handler.whenCalled('startQueryClusters'))[0];
    assertEquals(query, '');
    handler.reset();

    return clustersElement;
  }

  // Set up some test clusters used by the tests below. These are intentionally
  // all 1 visit clusters so that all elements have the same height so that we
  // can predicatably scroll to a certain px value before the end and have the
  // average item height, which is used to compute scroll height, not change.
  const visit1 = getTestVisit(
      {url: {url: 'www.chromium.org'}, visitTime: {internalValue: BigInt(1)}});
  const visit2 = getTestVisit({
    url: {url: 'chrome://extensions'},
    visitTime: {internalValue: BigInt(2)},
  });
  const visit3 = getTestVisit(
      {url: {url: 'chrome://settings'}, visitTime: {internalValue: BigInt(3)}});
  const cluster1: Cluster = getTestCluster(BigInt(111), [visit1]);
  const cluster2: Cluster = getTestCluster(BigInt(222), [visit2]);
  const cluster3: Cluster = getTestCluster(BigInt(333), [visit3]);

  test('Cluster removed loads more clusters', async () => {
    const clustersElement = await setupScrollableClustersElement();
    // Initial result doesn't fill the entire height, so more clusters are
    // requested.
    callbackRouterRemote.onClustersQueryResult({
      query: '',
      clusters: [cluster1],
      canLoadMore: true,
      isContinuation: false,
    });
    await handler.whenCalled('loadMoreClusters');

    const continuationResult: QueryResult = {
      query: '',
      clusters: [cluster2, cluster3],
      canLoadMore: true,
      isContinuation: true,
    };
    callbackRouterRemote.onClustersQueryResult(continuationResult);
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(1, handler.getCallCount('loadMoreClusters'));
    handler.reset();

    assertEquals(
        3,
        clustersElement.shadowRoot.querySelectorAll('history-cluster').length);

    // Visit 1 and visit 2 are removed. This opens up new space for more
    // clusters to be loaded, so more clusters should be requested.
    callbackRouterRemote.onVisitsRemoved([visit1]);
    await Promise.all([
      callbackRouterRemote.$.flushForTesting(),
      eventToPromise('remove-cluster', clustersElement),
    ]);
    callbackRouterRemote.onVisitsRemoved([visit2]);
    await Promise.all([
      callbackRouterRemote.$.flushForTesting(),
      eventToPromise('remove-cluster', clustersElement),
    ]);
    await handler.whenCalled('loadMoreClusters');

    callbackRouterRemote.onClustersQueryResult(
        Object.assign(continuationResult, {canLoadMore: false}));
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(1, handler.getCallCount('loadMoreClusters'));

    // Cluster 1 is removed since it contained only 1 visit, which matched the
    // removed one. 1 more cluster is added for a total of 3.
    assertEquals(
        3,
        clustersElement.shadowRoot.querySelectorAll('history-cluster').length);
  });

  // TODO(crbug.com/407488107): Fix flakiness and enable test.
  test.skip('Scroll to load more clusters', async () => {
    const clustersElement = await setupScrollableClustersElement();

    // Set up some test data. We intentionally load a lot of clusters for this
    // test so that the scroll height will be much larger than 300px.
    const clusters = [];
    for (let i = 0; i < 10; i++) {
      clusters.push(...[cluster1, cluster2, cluster3]);
    }
    callbackRouterRemote.onClustersQueryResult({
      query: '',
      clusters: clusters,
      canLoadMore: true,
      isContinuation: false,
    });
    await microtasksFinished();
    await new Promise(resolve => requestIdleCallback(resolve));

    const scrollTarget = document.body.querySelector('div');
    assertTrue(!!scrollTarget);

    // Scrolls the scrollTarget until there is lowerPx amount of space left to
    // scroll. Scrolling itself can cause cr-lazy-list to render items and cause
    // changes in scroll height, so this recursively scrolls until the lowerPx
    // is stable.
    const scrollUntilLower = async (lowerPx: number) => {
      assertGT(
          scrollTarget?.scrollHeight, scrollTarget.offsetHeight + lowerPx,
          'Scroll target is not tall enough.');
      const prevScrollHeight = scrollTarget.scrollHeight;
      scrollTarget.scrollTop =
          scrollTarget.scrollHeight - scrollTarget.offsetHeight - lowerPx;
      await eventToPromise('items-rendered', clustersElement);
      await microtasksFinished();
      if (scrollTarget.scrollHeight !== prevScrollHeight) {
        await scrollUntilLower(lowerPx);
      }
    };

    await scrollUntilLower(600);
    // Wait longer than scroll debounce in history-clusters.
    await new Promise(resolve => setTimeout(resolve, 10));
    assertEquals(0, handler.getCallCount('loadMoreClusters'));

    // Scroll to within 500px of the scroll height. More clusters should be
    // requested.
    await scrollUntilLower(490);
    await handler.whenCalled('loadMoreClusters');

    // Simulate more clusters loaded.
    callbackRouterRemote.onClustersQueryResult({
      query: '',
      clusters: [cluster1, cluster2, cluster3],
      canLoadMore: true,
      isContinuation: true,
    });
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(1, handler.getCallCount('loadMoreClusters'));
    handler.reset();

    // If history-clusters isn't active, scrolling doesn't load more. In prod
    // code this value is only set if the clusters are not visible (e.g. not
    // the active tab in the chrome://history UI).
    assertGT(
        scrollTarget.scrollHeight,
        scrollTarget.offsetHeight + scrollTarget.scrollTop + 500);
    clustersElement.isActive = false;
    await microtasksFinished();
    await scrollUntilLower(490);
    // Wait longer than scroll debounce.
    await new Promise(resolve => setTimeout(resolve, 10));
    assertEquals(0, handler.getCallCount('loadMoreClusters'));
  });

  test('Resize to load more clusters', async () => {
    const clustersElement = await setupScrollableClustersElement();
    clustersElement.setScrollDebounceForTest(1);

    // This result should fill the default 300px height.
    callbackRouterRemote.onClustersQueryResult({
      query: '',
      clusters: [
        cluster1,
        cluster2,
        cluster3,
        cluster1,
        cluster2,
        cluster3,
        cluster1,
      ],
      canLoadMore: true,
      isContinuation: false,
    });
    // Wait longer than scroll debounce.
    await new Promise(resolve => requestIdleCallback(resolve));
    assertEquals(0, handler.getCallCount('loadMoreClusters'));

    // Resize the clusters element by resizing its parent (simulates resizing
    // browser window). More clusters should be requested.
    document.body.style.maxHeight = '1000px';
    document.body.style.height = '1000px';
    await handler.whenCalled('loadMoreClusters');
    assertEquals(1, handler.getCallCount('loadMoreClusters'));
  });
});
