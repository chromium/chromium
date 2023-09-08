// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Cluster, InteractionState, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {LayoutType} from 'chrome://new-tab-page/history_clusters_layout_type.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters_v2.mojom-webui.js';
import {DismissModuleInstanceEvent, HistoryClustersProxyImplV2, historyClustersV2Descriptor, HistoryClustersV2ModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
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

function removeHrefAndClick(element: HTMLElement) {
  element.removeAttribute('href');
  element.click();
}

suite('NewTabPageModulesHistoryClustersV2ModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImplV2.setInstance(
            new HistoryClustersProxyImplV2(mock)));
    metrics = fakeMetricsPrivate();
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

      for (let i = 0; i < instanceCount; i++) {
        assertDeepEquals(
            [LayoutType.kImages, BigInt(i)],
            handler.getArgs('recordLayoutTypeShown')[i]);
      }
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
      assertTrue(!!$$(headerElement, 'ntp-module-header-v2'));
    });

    test('Header info button click opens info dialog', async () => {
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: '"Sample Journey"'})]);
      const moduleElement = moduleElements[0];
      assertTrue(!!moduleElement);

      const headerElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);
      headerElement!.dispatchEvent(new Event('info-button-click'));

      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });

    test(
        'Search suggestion header contains chip', async () => {
          // Arrange.
          loadTimeData.overrideValues({
            historyClustersSuggestionChipHeaderEnabled: true,
          });
          const moduleElements = await initializeModule(
              [createSampleCluster(2, {label: '"Sample Journey"'})]);
          const moduleElement = moduleElements[0];
          assertTrue(!!moduleElement);

          // Assert.
          const headerElement = $$(moduleElement, 'history-clusters-header-v2');
          assertTrue(!!headerElement);
          const suggestionChip = $$(headerElement, '#suggestion-chip');
          assertTrue(!!suggestionChip);
          assertEquals((suggestionChip as HTMLElement).hidden, false);
        });

    test('Search suggestion header click triggers navigation', async () => {
      loadTimeData.overrideValues({
        historyClustersSuggestionChipHeaderEnabled: true,
      });
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: '"Sample Journey"'})]);
      const moduleElement = moduleElements[0];

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);
      const suggestionChip = $$<HTMLElement>(headerElement, '#suggestion-chip');
      assertTrue(!!suggestionChip);

      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      removeHrefAndClick(suggestionChip);
      await waitForUsageEvent;
    });

    test(
        'Backend is notified when module is dismissed and restored',
        async () => {
          // Arrange.
          const sampleCluster =
              createSampleCluster(2, {label: '"Sample Journey"'});
          const moduleElements = await initializeModule([sampleCluster]);
          const moduleElement = moduleElements[0];
          assertTrue(!!moduleElement);

          // Act.
          const waitForDismissEvent =
              eventToPromise('dismiss-module-instance', moduleElement);
          const dismissButton =
              moduleElement.shadowRoot!
                  .querySelector('history-clusters-header-v2')!.shadowRoot!
                  .querySelector('ntp-module-header-v2')!.shadowRoot!
                  .querySelector('#dismiss')! as HTMLButtonElement;
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

    test(
        'Backend is notified when module is marked done and restored',
        async () => {
          // Arrange.
          const sampleCluster =
              createSampleCluster(2, {label: '"Sample Journey"'});
          const moduleElements = await initializeModule([sampleCluster]);
          const moduleElement = moduleElements[0];
          assertTrue(!!moduleElement);

          // Act.
          const waitForDismissEvent =
              eventToPromise('dismiss-module-instance', moduleElement);
          const doneButton =
              moduleElement.shadowRoot!
                  .querySelector('history-clusters-header-v2')!.shadowRoot!
                  .querySelector('ntp-module-header-v2')!.shadowRoot!
                  .querySelector('#done')! as HTMLButtonElement;
          doneButton.click();

          // Assert.
          const dismissEvent: DismissModuleInstanceEvent =
              await waitForDismissEvent;
          assertEquals(
              `${sampleCluster.label!} hidden`, dismissEvent.detail.message);
          assertTrue(!!dismissEvent.detail.restoreCallback);
          assertUpdateClusterVisitsInteractionStateCall(
              InteractionState.kDone, 3);

          // Act.
          const restoreCallback = dismissEvent.detail.restoreCallback!;
          restoreCallback();

          // Assert.
          assertUpdateClusterVisitsInteractionStateCall(
              InteractionState.kDefault, 3);
        });

    test('Show History side panel invoked when clicking header', async () => {
      loadTimeData.overrideValues({
        historyClustersSuggestionChipHeaderEnabled: false,
      });

      const sampleClusterLabel = '"Sample Journey"';
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: sampleClusterLabel})]);
      const moduleElement = moduleElements[0];
      assertTrue(!!moduleElement);
      const headerElement =
          $$<HTMLElement>(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);
      const buttonElement = $$<HTMLElement>(headerElement, 'button');
      assertTrue(!!buttonElement);

      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      buttonElement.click();

      assertEquals(
          sampleClusterLabel.substring(1, sampleClusterLabel.length - 1),
          handler.getArgs('showJourneysSidePanel')[0]);
      await waitForUsageEvent;
    });

    test(
        'Show History side panel is invoked when performing show all action',
        async () => {
          const sampleClusterLabel = '"Sample Journey"';
          const moduleElements = await initializeModule(
              [createSampleCluster(2, {label: sampleClusterLabel})]);
          const moduleElement = moduleElements[0];
          assertTrue(!!moduleElement);
          const headerElement = $$(moduleElement, 'history-clusters-header-v2');
          assertTrue(!!headerElement);

          const waitForUsageEvent = eventToPromise('usage', moduleElement);
          headerElement!.dispatchEvent(new Event('show-all-button-click'));

          assertEquals(
              sampleClusterLabel.substring(1, sampleClusterLabel.length - 1),
              handler.getArgs('showJourneysSidePanel')[0]);
          await waitForUsageEvent;
        });

    [['Visit', 'ntp-history-clusters-visit-tile'],
     ['Suggest', 'ntp-history-clusters-suggest-tile-v2']]
        .forEach(([type, tileTagName]) => {
          test(`Tile click metrics for ${type}`, async () => {
            const sampleClusterLabel = '"Sample Journey"';
            const moduleElements = await initializeModule(
                [createSampleCluster(2, {label: sampleClusterLabel})]);
            const moduleElement = moduleElements[0];
            assertTrue(!!moduleElement);
            const tileElement = $$<HTMLElement>(moduleElement, tileTagName!);
            assertTrue(!!tileElement);

            const waitForUsageEvent = eventToPromise('usage', moduleElement);
            tileElement.click();

            assertEquals(BigInt(111), handler.getArgs('recordClick')[0]);
            assertEquals(
                1,
                metrics.count(`NewTabPage.HistoryClusters.Layout${
                    LayoutType.kImages}.${type}Tile.ClickIndex`));

            await waitForUsageEvent;
          });
        });

    [...Array(3).keys()].forEach(numRelatedSearches => {
      test('Module shows correct amount of related searches', async () => {
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
