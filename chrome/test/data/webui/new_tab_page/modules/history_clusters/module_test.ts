// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Cluster, RawVisitData, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters.mojom-webui.js';
import {historyClustersDescriptor, HistoryClustersModuleElement, HistoryClustersProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesHistoryClustersModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImpl.setInstance(
            new HistoryClustersProxyImpl(mock)));
  });

  function createSampleCluster(overrides?: Partial<Cluster>): Cluster {
    const rawVisitData: RawVisitData = {
      url: {url: ''},
      visitTime: {internalValue: BigInt(0)},
    };

    const urlVisit1: URLVisit = {
      visitId: BigInt(1),
      normalizedUrl: {url: 'https://www.google.com'},
      urlForDisplay: 'https://www.google.com',
      pageTitle: 'Test Title',
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
    handler.setResultFor('getCluster', Promise.resolve({cluster: null}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('getCluster');
    assertEquals(null, moduleElement);
  });

  test('Module created when history cluster data available', async () => {
    // Arrange.
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster()}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('getCluster');
    assertTrue(!!moduleElement);
  });

  test('Tile element populated with correct data', async () => {
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster()}));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    document.body.append(moduleElement);
    assertTrue(!!moduleElement);
    await handler.whenCalled('getCluster');
    await waitAfterNextRender(moduleElement);

    const tileElement = $$(moduleElement, 'ntp-history-clusters-tile');
    assertTrue(!!tileElement);

    assertEquals($$(tileElement, '#title')!.innerHTML, 'Test Title');
  });
});
