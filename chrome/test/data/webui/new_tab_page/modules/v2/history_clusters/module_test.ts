// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Cart} from 'chrome://new-tab-page/cart.mojom-webui.js';
import {Cluster} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters.mojom-webui.js';
import {HistoryClustersProxyImpl, historyClustersV2Descriptor, HistoryClustersV2ModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../../../test_support.js';
import {assertModuleHeaderTitle, createRelatedSearches, createSampleVisits} from '../../history_clusters/test_support.js';

function createSampleClusters(count: number): Cluster[] {
  return new Array(count).fill(0).map(
      (_, i) => createSampleCluster(2, {id: BigInt(i)}));
}

function createSampleCluster(
    numRelatedSearches: number,
    overrides?: Partial<Cluster>,
    ): Cluster {
  const cluster: Cluster = Object.assign(
      {
        id: BigInt(111),
        visits: createSampleVisits(2, 2),
        label: '',
        labelMatchPositions: [],
        relatedSearches: createRelatedSearches(numRelatedSearches),
        imageUrl: undefined,
        fromPersistence: false,
        debugInfo: undefined,
      },
      overrides);

  return cluster;
}

suite('NewTabPageModulesHistoryClustersV2ModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImpl.setInstance(
            new HistoryClustersProxyImpl(mock)));
  });

  async function initializeModule(clusters: Cluster[], cart: Cart|null = null):
      Promise<HistoryClustersV2ModuleElement[]> {
    handler.setResultFor('getClusters', Promise.resolve({clusters}));
    handler.setResultFor('getCartForCluster', Promise.resolve({cart}));
    const moduleElements = await historyClustersV2Descriptor.initialize(0) as
        HistoryClustersV2ModuleElement[];
    if (moduleElements) {
      moduleElements.forEach(element => {
        document.body.append(element);
      });
    }

    await waitAfterNextRender(document.body);
    return moduleElements;
  }

  suite('core', () => {
    test('No module created if no history cluster data', async () => {
      // Arrange.
      const moduleElements = await initializeModule([]);

      // Assert.
      assertEquals(null, moduleElements);
    });

    test('Multiple module instances created successfully', async () => {
      const instanceCount = 3;
      const moduleElements =
          await initializeModule(createSampleClusters(instanceCount));
      assertEquals(instanceCount, moduleElements.length);
    });

    test('Header element populated with correct data', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: sampleClusterLabel})]);
      const moduleElement = moduleElements[0];

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);

      assertModuleHeaderTitle(headerElement, `${sampleClusterLabel}`);
    });

    test('Header info button click opens info dialog', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: sampleClusterLabel})]);
      const moduleElement = moduleElements[0];

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header-v2');
      assertTrue(!!headerElement);

      headerElement!.dispatchEvent(new Event('info-button-click'));

      // Assert.
      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });
  });
});
