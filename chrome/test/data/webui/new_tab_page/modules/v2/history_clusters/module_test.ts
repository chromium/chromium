// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Cluster, InteractionState, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters_v2.mojom-webui.js';
import {DismissModuleInstanceEvent, HistoryClustersProxyImplV2, historyClustersV2Descriptor, HistoryClustersV2ModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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
        mock => HistoryClustersProxyImplV2.setInstance(
            new HistoryClustersProxyImplV2(mock)));
  });

  async function initializeModule(clusters: Cluster[]):
      Promise<HistoryClustersV2ModuleElement[]> {
    handler.setResultFor('getClusters', Promise.resolve({clusters}));
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

  async function assertUpdateClusterVisitsInteractionStateCall(
      state: InteractionState, count: number) {
    const [visits, interactionState] =
        await handler.whenCalled('updateClusterVisitsInteractionState');

    assertEquals(count, visits.length);
    visits.forEach((visit: URLVisit, index: number) => {
      assertEquals(index, Number(visit.visitId));
    });
    assertEquals(state, interactionState);
  }

  suite('Core', () => {
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
      const headerElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);
      const label = $$(headerElement, '#label');
      assertTrue(!!label);

      assertModuleHeaderTitle(label as HTMLElement, `${sampleClusterLabel}`);
    });

    test('Header info button click opens info dialog', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: sampleClusterLabel})]);
      const moduleElement = moduleElements[0];

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);

      headerElement!.dispatchEvent(new Event('info-button-click'));

      // Assert.
      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });

    test('Header contains label that is not hidden', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      loadTimeData.overrideValues({
        historyClustersSuggestionChipHeaderEnabled: true,
      });
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: sampleClusterLabel})]);
      const moduleElement = moduleElements[0];

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);
      const label = $$(headerElement, '#label');
      assertTrue(!!label);
      const suggestionChip = $$(headerElement, '#suggestion-chip');
      assertTrue(!!suggestionChip);

      // Assert.
      assertEquals((label as HTMLElement).hidden, true);
      assertEquals((suggestionChip as HTMLElement).hidden, false);
    });

    test(
        'Backend is notified when module is dismissed and restored',
        async () => {
          // Arrange.
          const sampleClusterLabel = '"Sample Journey"';
          const sampleCluster =
              createSampleCluster(2, {label: sampleClusterLabel});
          const moduleElements = await initializeModule([sampleCluster]);
          const moduleElement = moduleElements[0];
          assertTrue(!!moduleElement);

          // Act.
          const waitForDismissEvent =
              eventToPromise('dismiss-module-instance', moduleElement);
          const dismissButton =
              moduleElement.shadowRoot!
                  .querySelector('history-clusters-header-v2')!.shadowRoot!
                  .querySelector<HTMLElement>('#dismissButton')!;
          dismissButton.click();

          // Assert.
          const dismissEvent: DismissModuleInstanceEvent =
              await waitForDismissEvent;
          assertEquals(
              `${sampleCluster.label!} hidden`, dismissEvent.detail.message);
          assertTrue(!!dismissEvent.detail.restoreCallback);
          assertUpdateClusterVisitsInteractionStateCall(
              InteractionState.kHidden, 3);

          // Act.
          const restoreCallback = dismissEvent.detail.restoreCallback!;
          restoreCallback();

          // Assert.
          assertUpdateClusterVisitsInteractionStateCall(
              InteractionState.kDefault, 3);
        });

    [...Array(3).keys()].forEach(numRelatedSearches => {
      test('module shows correct amount of related searches', async () => {
        // Arrange.
        const sampleClusterLabel = '"Sample Journey"';
        const moduleElements = await initializeModule([createSampleCluster(
            numRelatedSearches, {label: sampleClusterLabel})]);
        const moduleElement = moduleElements[0];

        // Assert.
        assertTrue(!!moduleElement);
        const relatedSearchesElement =
            $$(moduleElement, '#related-searches') as HTMLElement;
        assertTrue(!!relatedSearchesElement);
        assertEquals((numRelatedSearches < 2), relatedSearchesElement.hidden);
      });
    });
  });
});
