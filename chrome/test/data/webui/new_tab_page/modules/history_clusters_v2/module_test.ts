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

import {installMock} from '../../test_support.js';
import {assertModuleHeaderTitle, createRelatedSearches, createSampleVisits} from '../history_clusters/test_support.js';

function createSampleCluster(
    numRelatedSearches?: number, overrides?: Partial<Cluster>): Cluster {
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
      Promise<HistoryClustersV2ModuleElement> {
    handler.setResultFor('getClusters', Promise.resolve({clusters}));
    handler.setResultFor('getCartForCluster', Promise.resolve({cart}));
    const moduleElement = await historyClustersV2Descriptor.initialize(0) as
        HistoryClustersV2ModuleElement;
    await handler.whenCalled('getClusters');
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);
    return moduleElement;
  }

  suite('core', () => {
    test('No module created if no history cluster data', async () => {
      // Arrange.
      const moduleElement = await initializeModule([]);

      // Assert.
      assertEquals('', moduleElement.innerHTML);
    });

    test('No module created when data does not match layouts', async () => {
      // Arrange.
      const cluster: Partial<Cluster> = {
        visits: createSampleVisits(2, 0),
      };
      const moduleElement =
          await initializeModule([createSampleCluster(undefined, cluster)]);

      // Assert.
      assertEquals('', moduleElement.innerHTML);
    });

    test('Header element populated with correct data', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElement = await initializeModule(
          [createSampleCluster(undefined, {label: sampleClusterLabel})]);

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);

      assertModuleHeaderTitle(headerElement, sampleClusterLabel);
    });

    test('Header info button click opens info dialog', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElement = await initializeModule(
          [createSampleCluster(undefined, {label: sampleClusterLabel})]);

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);

      headerElement!.dispatchEvent(new Event('info-button-click'));

      // Assert.
      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });
  });
});
