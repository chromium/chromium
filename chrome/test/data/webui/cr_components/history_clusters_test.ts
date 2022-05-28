// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import {HistoryClustersElement} from 'chrome://resources/cr_components/history_clusters/clusters.js';
import {Cluster, PageCallbackRouter, PageHandlerRemote, QueryResult} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

let handler: PageHandlerRemote&TestBrowserProxy;
let callbackRouterRemote: PageCallbackRouter;

function createBrowserProxy() {
  handler = TestBrowserProxy.fromClass(PageHandlerRemote);
  const callbackRouter = new PageCallbackRouter();
  BrowserProxyImpl.setInstance(new BrowserProxyImpl(handler, callbackRouter));
  callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
}

suite('history-clusters', () => {
  setup(() => {
    document.body.innerHTML = '';

    createBrowserProxy();
  });

  test('Basic end-to-end', async () => {
    const clustersElement = new HistoryClustersElement();
    document.body.appendChild(clustersElement);

    const query = await handler.whenCalled('startQueryClusters');
    assertEquals(query, '');

    const cluster1 = new Cluster();
    cluster1.visits = [];
    cluster1.labelMatchPositions = [];
    cluster1.relatedSearches = [];
    const cluster2 = new Cluster();
    cluster2.visits = [];
    cluster2.labelMatchPositions = [];
    cluster2.relatedSearches = [];

    const queryResult = new QueryResult();
    queryResult.query = '';
    queryResult.clusters = [cluster1, cluster2];

    callbackRouterRemote.onClustersQueryResult(queryResult);
    await callbackRouterRemote.$.flushForTesting();
    flushTasks();

    const ironListItems = clustersElement.$.clusters.items!;
    assertEquals(ironListItems.length, 2);
  });
});
