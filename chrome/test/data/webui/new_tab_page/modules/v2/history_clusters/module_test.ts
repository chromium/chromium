// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Discount} from 'chrome://new-tab-page/discount.mojom-webui.js';
import {Cluster, InteractionState, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {LayoutType} from 'chrome://new-tab-page/history_clusters_layout_type.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters_v2.mojom-webui.js';
import {DismissModuleInstanceEvent, HistoryClustersProxyImplV2, historyClustersV2Descriptor, HistoryClustersV2ModuleElement, HistoryClusterV2ImageDisplayState, PageImageServiceBrowserProxy, VisitTileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
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

function checkInfoDialogContent(
    moduleElement: HistoryClustersV2ModuleElement, id: string) {
  const expectedInfo =
      loadTimeData.getString(id).replaceAll('\n', '').replaceAll('<br>', '');
  const actualInfo =
      moduleElement.$.infoDialogRender.get().textContent!.replaceAll('\n', '');
  assertEquals(expectedInfo, actualInfo);
}

suite('NewTabPageModulesHistoryClustersV2ModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImplV2.setInstance(
            new HistoryClustersProxyImplV2(mock)));
    imageServiceHandler = installMock(
        PageImageServiceHandlerRemote,
        mock => PageImageServiceBrowserProxy.setInstance(
            new PageImageServiceBrowserProxy(mock)));
    metrics = fakeMetricsPrivate();
  });

  async function initializeModule(
      clusters: Cluster[],
      discounts: Map<Url, Discount[]> = new Map<Url, Discount[]>()):
      Promise<HistoryClustersV2ModuleElement[]> {
    handler.setResultFor('getClusters', Promise.resolve({clusters}));
    handler.setResultFor('getCartForCluster', Promise.resolve({cart: null}));
    handler.setResultFor(
        'getDiscountsForCluster', Promise.resolve({discounts}));
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
      const clusterLabel = 'Sample Journey';
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: `"${clusterLabel}"`})]);
      const moduleElement = moduleElements[0];

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerElement);
      const label = $$(headerElement, '#label');
      assertTrue(!!label);
      assertModuleHeaderTitle(label as HTMLElement, `${clusterLabel}`);
      assertTrue(!!$$(headerElement, 'ntp-module-header-v2'));
    });

    test('Header element has expected action menu items', async () => {
      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: '"Sample Journey"'})]);
      const moduleElement = moduleElements[0];
      assertTrue(!!moduleElement);

      const headerTileElement = $$(moduleElement, 'history-clusters-header-v2');
      assertTrue(!!headerTileElement);
      const moduleHeaderElement = $$(headerTileElement, 'ntp-module-header-v2');
      assertTrue(!!moduleHeaderElement);
      const actionMenu = $$(moduleHeaderElement, 'cr-action-menu');
      assertTrue(!!actionMenu);

      const actionMenuItems =
          [...actionMenu.querySelectorAll('button.dropdown-item')];
      assertEquals(6, actionMenuItems.length);
      ['done', 'dismiss', 'disable', 'show-all', 'info', 'customize-module']
          .forEach((action, index) => {
            assertEquals(
                action, actionMenuItems[index]!.getAttribute('data-action'));
          });
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

    test('Header done button dipatches dismiss module event', async () => {
      const sampleCluster = createSampleCluster(2, {label: '"Sample"'});
      const moduleElements = await initializeModule([sampleCluster]);
      const moduleElement = moduleElements[0];
      assertTrue(!!moduleElement);

      const waitForDismissEvent =
          eventToPromise('dismiss-module-instance', moduleElement);
      const doneButton =
          moduleElement.shadowRoot!.querySelector('history-clusters-header-v2')!
              .shadowRoot!.querySelector(
                  'ntp-module-header-v2 cr-icon-button')! as HTMLElement;
      doneButton.click();

      const dismissEvent: DismissModuleInstanceEvent =
          await waitForDismissEvent;
      assertEquals(
          `${sampleCluster.label!} hidden`, dismissEvent.detail.message);
      assertUpdateClusterVisitsInteractionStateCall(InteractionState.kDone, 3);
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
        checkInfoDialogContent(moduleElement, 'modulesJourneysInfo');
      });
    });
  });

  suite('PagehideMetricNoImages', () => {
    test('Module records no images state metric on pagehide', async () => {
      imageServiceHandler.setResultFor(
          'getPageImageUrl', Promise.resolve(null));

      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: '"Sample"'})],
      );
      const moduleElement = moduleElements[0];
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);

      window.dispatchEvent(new Event('pagehide'));

      assertEquals(2, imageServiceHandler.getCallCount('getPageImageUrl'));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.ImageDisplayState`,
              HistoryClusterV2ImageDisplayState.NONE));
    });
  });

  suite('PagehideMetricAllImages', () => {
    test('Module records all images state metric on pagehide', async () => {
      imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
        result: {imageUrl: {url: 'https://example.com/image.png'}},
      }));

      const moduleElements = await initializeModule(
          [createSampleCluster(2, {label: '"Sample"'})],
      );
      const moduleElement = moduleElements[0];
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);

      window.dispatchEvent(new Event('pagehide'));

      assertEquals(2, imageServiceHandler.getCallCount('getPageImageUrl'));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.ImageDisplayState`,
              HistoryClusterV2ImageDisplayState.ALL));
    });
  });

  suite('Discounts', () => {
    test('Discount is not initialized when feature is disabled', async () => {
      loadTimeData.overrideValues({
        historyClustersModuleDiscountsEnabled: false,
      });

      const instanceCount = 3;
      const moduleElements =
          await initializeModule(createSampleClusters(instanceCount));
      assertEquals(instanceCount, moduleElements.length);

      assertEquals(0, handler.getCallCount('getDiscountsForCluster'));
      for (const moduleElement of moduleElements) {
        assertTrue(!!moduleElement);
        await waitAfterNextRender(moduleElement);
        for (const discount of moduleElement.discounts) {
          assertEquals('', discount);
        }
        const contentElement =
            moduleElement.shadowRoot!
                .querySelector('ntp-history-clusters-visit-tile')!.shadowRoot!
                .querySelector('#content')! as HTMLElement;
        assertEquals(
            contentElement.getAttribute('aria-label'),
            'Test Title 1, foo.com, 1 min ago');
      }
      assertEquals(0, metrics.count(`NewTabPage.HistoryClusters.HasDiscount`));
    });

    test('Discount initialization', async () => {
      loadTimeData.overrideValues({
        historyClustersModuleDiscountsEnabled: true,
      });

      const instanceCount = 2;
      const visitCount = 3;
      const clusters = createSampleClusters(instanceCount);
      assertEquals(clusters.length, instanceCount);
      assertEquals(clusters[0]!.visits.length, visitCount);
      assertEquals(clusters[1]!.visits.length, visitCount);
      // Update clusters visit URLs so that they are different. Skip the first
      // visit since it is the SRP.
      clusters[0]!.visits[1]!.normalizedUrl = {url: 'https://www.foo.com/1'};
      clusters[0]!.visits[2]!.normalizedUrl = {url: 'https://www.foo.com/2'};
      clusters[1]!.visits[1]!.normalizedUrl = {url: 'https://www.foo.com/3'};
      clusters[1]!.visits[2]!.normalizedUrl = {url: 'https://www.foo.com/4'};

      const discoutMap = new Map<Url, Discount[]>();
      discoutMap.set(clusters[0]!.visits[1]!.normalizedUrl, [{
                       valueInText: '15% off',
                       annotatedVisitUrl: {url: 'https://www.annotated.com/1'},
                     }]);
      discoutMap.set(clusters[1]!.visits[2]!.normalizedUrl, [{
                       valueInText: '$10 off',
                       annotatedVisitUrl: {url: 'https://www.annotated.com/2'},
                     }]);

      const moduleElements = await initializeModule(clusters, discoutMap);
      assertEquals(
          instanceCount, handler.getCallCount('getDiscountsForCluster'));
      for (const moduleElement of moduleElements) {
        assertTrue(!!moduleElement);
        await waitAfterNextRender(moduleElement);
        assertEquals(moduleElement.discounts.length, visitCount);
      }
      assertEquals(2, metrics.count(`NewTabPage.HistoryClusters.HasDiscount`));

      // Assert Module One.
      const expectedDiscountsModuleOne = ['', '15% off', ''];
      let visitTiles: VisitTileModuleElement[] =
          Array.from(moduleElements[0]!.shadowRoot!.querySelectorAll(
              'ntp-history-clusters-visit-tile'));
      assertEquals(visitTiles.length, visitCount - 1);
      for (let i = 0; i < moduleElements[0]!.discounts.length; i++) {
        assertEquals(
            expectedDiscountsModuleOne[i], moduleElements[0]!.discounts[i]);
        // Skip the first one which is SRP.
        if (i !== 0) {
          assertEquals(
              expectedDiscountsModuleOne[i], visitTiles[i - 1]!.discount);
        }
      }
      assertEquals(
          'https://www.annotated.com/1',
          visitTiles[0]!.visit.normalizedUrl.url);
      let contentElement =
          visitTiles[0]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 1, annotated.com, 1 min ago, 15% off');

      assertEquals(
          'https://www.foo.com/2', visitTiles[1]!.visit.normalizedUrl.url);
      contentElement =
          visitTiles[1]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 2, foo.com, 1 min ago');

      // Assert Module Two.
      const expectedDiscountsModuleTwo = ['', '', '$10 off'];
      visitTiles = Array.from(moduleElements[1]!.shadowRoot!.querySelectorAll(
          'ntp-history-clusters-visit-tile'));
      assertEquals(visitTiles.length, visitCount - 1);
      for (let i = 0; i < moduleElements[1]!.discounts.length; i++) {
        assertEquals(
            expectedDiscountsModuleTwo[i], moduleElements[1]!.discounts[i]);
        // Skip the first one which is SRP.
        if (i !== 0) {
          assertEquals(
              expectedDiscountsModuleTwo[i], visitTiles[i - 1]!.discount);
        }
      }
      assertEquals(
          'https://www.foo.com/3', visitTiles[0]!.visit.normalizedUrl.url);
      contentElement =
          visitTiles[0]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 1, foo.com, 1 min ago');

      assertEquals(
          'https://www.annotated.com/2',
          visitTiles[1]!.visit.normalizedUrl.url);
      contentElement =
          visitTiles[1]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 2, annotated.com, 1 min ago, $10 off');

      // Assert info dialog.
      for (const moduleElement of moduleElements) {
        checkInfoDialogContent(moduleElement, 'modulesHistoryWithDiscountInfo');
      }
    });

    test('Metrics for Discount click', async () => {
      loadTimeData.overrideValues({
        historyClustersModuleDiscountsEnabled: true,
      });

      const instanceCount = 1;
      const visitCount = 3;
      const clusters = createSampleClusters(instanceCount);
      const discoutMap = new Map<Url, Discount[]>();
      discoutMap.set(clusters[0]!.visits[1]!.normalizedUrl, [{
                       valueInText: '15% off',
                       annotatedVisitUrl: {url: 'https://www.annotated.com/1'},
                     }]);

      const moduleElements = await initializeModule(clusters, discoutMap);
      assertEquals(
          instanceCount, handler.getCallCount('getDiscountsForCluster'));
      for (const moduleElement of moduleElements) {
        assertTrue(!!moduleElement);
        await waitAfterNextRender(moduleElement);
        assertEquals(moduleElement.discounts.length, visitCount);
      }
      assertEquals(1, metrics.count(`NewTabPage.HistoryClusters.HasDiscount`));

      const visitTiles: VisitTileModuleElement[] =
          Array.from(moduleElements[0]!.shadowRoot!.querySelectorAll(
              'ntp-history-clusters-visit-tile'));
      assertEquals(visitTiles.length, visitCount - 1);

      visitTiles[1]!.click();
      assertEquals(
          0, metrics.count(`NewTabPage.HistoryClusters.DiscountClicked`));

      visitTiles[0]!.click();
      assertEquals(
          1, metrics.count(`NewTabPage.HistoryClusters.DiscountClicked`));
    });
  });
});
