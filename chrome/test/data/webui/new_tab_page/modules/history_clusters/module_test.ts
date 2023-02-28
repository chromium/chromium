// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Cluster, RawVisitData} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters.mojom-webui.js';
import {HistoryClusterLayoutType, historyClustersDescriptor, HistoryClustersModuleElement, HistoryClustersProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
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

  function createSampleCluster(
      numVisits: number = 1, numImageVisits: number = 0,
      overrides?: Partial<Cluster>): Cluster {
    const rawVisitData: RawVisitData = {
      url: {url: ''},
      visitTime: {internalValue: BigInt(0)},
    };

    const cluster: Cluster = Object.assign(
        {
          id: BigInt(111),
          visits: [],
          label: undefined,
          labelMatchPositions: [],
          relatedSearches: [],
          imageUrl: undefined,
          fromPersistence: false,
          debugInfo: undefined,
        },
        overrides);

    for (let i = 0; i < numVisits; i++) {
      cluster.visits.push({
        visitId: BigInt(i),
        normalizedUrl: {url: `https://www.google.com/${i}`},
        urlForDisplay: `www.google.com/${i}`,
        pageTitle: `Test Title ${i}`,
        titleMatchPositions: [],
        urlForDisplayMatchPositions: [],
        duplicates: [],
        relativeDate: '',
        annotations: [],
        debugInfo: {},
        rawVisitData: rawVisitData,
        hasUrlKeyedImage: i < numImageVisits,
        isKnownToSync: false,
      });
    }

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

  test('No module created when data does not match layouts', async () => {
    // Arrange.
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(2, 0)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('getCluster');
    assertEquals(null, moduleElement);
  });

  test('Layout 1 is used', async () => {
    // Arrange.
    // 3 total visits (2 + SRP) with 2 being image visits.
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(3, 2)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);

    // Assert.
    const layoutElements =
        moduleElement.shadowRoot!.querySelectorAll('.layout');
    assertEquals(HistoryClusterLayoutType.LAYOUT_1, moduleElement.layoutType);
    assertEquals(layoutElements.length, 1);
    assertEquals(layoutElements[0]!.id, 'layout1');
  });

  test('Layout 2 is used', async () => {
    // Arrange.
    // 4 total visits (3 + SRP) with 1 being an image visit.
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(4, 1)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);

    // Assert.
    const layoutElements =
        moduleElement.shadowRoot!.querySelectorAll('.layout');
    assertEquals(HistoryClusterLayoutType.LAYOUT_2, moduleElement.layoutType);
    assertEquals(layoutElements.length, 1);
    assertEquals(layoutElements[0]!.id, 'layout2');
  });

  test('Layout 3 is used', async () => {
    // Arrange.
    // 5 total visits (4 + SRP) with 2 being image visits.
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(5, 2)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);

    // Assert.
    const layoutElements =
        moduleElement.shadowRoot!.querySelectorAll('.layout');
    assertEquals(HistoryClusterLayoutType.LAYOUT_3, moduleElement.layoutType);
    assertEquals(layoutElements.length, 1);
    assertEquals(layoutElements[0]!.id, 'layout3');
  });

  test('Tile element populated with correct data', async () => {
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(3, 2)}));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    document.body.append(moduleElement);
    assertTrue(!!moduleElement);
    await handler.whenCalled('getCluster');
    await waitAfterNextRender(moduleElement);

    const tileElement = $$(moduleElement, 'ntp-history-clusters-tile');
    assertTrue(!!tileElement);

    assertEquals($$(tileElement, '#title')!.innerHTML, 'Test Title 0');
  });
});
