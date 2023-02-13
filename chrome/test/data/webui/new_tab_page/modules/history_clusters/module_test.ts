// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {historyClustersDescriptor, HistoryClustersModuleElement, HistoryClustersProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {Cluster, RawVisitData, URLVisit} from 'chrome://resources/cr_components/history_clusters/history_cluster_types.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote, PageRemote, QueryResult} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesHistoryClustersModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let callbackRouter: PageRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImpl.setInstance(
            new HistoryClustersProxyImpl(mock, new PageCallbackRouter())));
    callbackRouter = HistoryClustersProxyImpl.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  function createQueryResult(clusters: Cluster[]): QueryResult {
    return {
      clusters,
      query: '',
      canLoadMore: false,
      isContinuation: false,
    };
  }

  function createSampleCluster(overrides?: Partial<Cluster>): Cluster {
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
      imageUrl: undefined,
      isKnownToSync: false,
    };

    const cluster: Cluster = Object.assign(
        {
          id: BigInt(111),
          visits: [urlVisit1],
          label: undefined,
          labelMatchPositions: [],
          relatedSearches: [],
          imageUrl: undefined,
          fromPersistence: false,
          debugInfo: undefined,
        },
        overrides);

    return cluster;
  }

  test('No module created if no history cluster data', async () => {
    // Arrange.
    callbackRouter.onClustersQueryResult(createQueryResult([]));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('startQueryClusters');
    assertEquals(null, moduleElement);
  });

  test('Module created when history cluster data available', async () => {
    // Arrange.
    callbackRouter.onClustersQueryResult(
        createQueryResult([createSampleCluster()]));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('startQueryClusters');
    assertTrue(!!moduleElement);
  });
});
