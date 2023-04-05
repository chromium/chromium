// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Cluster, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters.mojom-webui.js';
import {DismissModuleEvent, HistoryClusterElementType, HistoryClusterImageDisplayState, HistoryClusterLayoutType, historyClustersDescriptor, HistoryClustersModuleElement, HistoryClustersProxyImpl, ImageServiceBrowserProxy, LAYOUT_1_MIN_IMAGE_VISITS, LAYOUT_1_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS, LAYOUT_2_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS, LAYOUT_3_MIN_VISITS, MIN_RELATED_SEARCHES} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ImageServiceHandlerRemote} from 'chrome://resources/cr_components/image_service/image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

import {createRelatedSearches, createSampleVisits, GOOGLE_SEARCH_BASE_URL} from './test_support.js';

const DISPLAY_LAYOUT_METRIC_NAME = 'NewTabPage.HistoryClusters.DisplayLayout';

function assertLayoutSet(
    moduleElement: HistoryClustersModuleElement,
    layoutType: HistoryClusterLayoutType) {
  const layoutElements = moduleElement.shadowRoot!.querySelectorAll('.layout');
  assertEquals(layoutType, moduleElement.layoutType);
  assertEquals(layoutElements.length, 1);
  assertEquals(layoutElements[0]!.id, `layout${layoutType}`);
}

function assertModuleHeaderTitle(headerElement: HTMLElement, title: string) {
  const moduleHeaderTextContent = headerElement.textContent!.trim();
  const headerText = moduleHeaderTextContent.split(/\r?\n/);
  assertTrue(headerText.length > 0);
  assertEquals(title, headerText[0]!.trim());
}

function createLayoutSuitableSampleVisits(
    layoutType: HistoryClusterLayoutType =
        HistoryClusterLayoutType.LAYOUT_1): URLVisit[] {
  switch (layoutType) {
    case HistoryClusterLayoutType.LAYOUT_1:
      return createSampleVisits(LAYOUT_1_MIN_VISITS, LAYOUT_1_MIN_IMAGE_VISITS);
    case HistoryClusterLayoutType.LAYOUT_2:
      return createSampleVisits(LAYOUT_2_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS);
    case HistoryClusterLayoutType.LAYOUT_3:
      return createSampleVisits(LAYOUT_3_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS);
  }
  return [];
}

function createSampleCluster(
    layout?: HistoryClusterLayoutType, numRelatedSearches?: number,
    overrides?: Partial<Cluster>): Cluster {
  const cluster: Cluster = Object.assign(
      {
        id: BigInt(111),
        visits: createLayoutSuitableSampleVisits(layout),
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

suite('NewTabPageModulesHistoryClustersModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let imageServiceHandler: TestMock<ImageServiceHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImpl.setInstance(
            new HistoryClustersProxyImpl(mock)));
    imageServiceHandler = installMock(
        ImageServiceHandlerRemote,
        mock => ImageServiceBrowserProxy.setInstance(
            new ImageServiceBrowserProxy(mock)));
    metrics = fakeMetricsPrivate();
  });

  async function initializeModule(cluster: Cluster|
                                  null): Promise<HistoryClustersModuleElement> {
    handler.setResultFor('getCluster', Promise.resolve({cluster: cluster}));
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    await handler.whenCalled('getCluster');
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);
    return moduleElement;
  }

  suite('core', () => {
    test('No module created if no history cluster data', async () => {
      // Arrange.
      const moduleElement = await initializeModule(null);

      // Assert.
      assertEquals(null, moduleElement);
      assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
      assertEquals(
          1,
          metrics.count(
              DISPLAY_LAYOUT_METRIC_NAME, HistoryClusterLayoutType.NONE));
    });

    test('No module created when data does not match layouts', async () => {
      // Arrange.
      const cluster: Partial<Cluster> = {
        visits: createSampleVisits(2, 0),
      };
      const moduleElement = await initializeModule(createSampleCluster(
          HistoryClusterLayoutType.NONE, undefined, cluster));

      // Assert.
      assertEquals(null, moduleElement);
    });

    test('No module created when less than min related searches', async () => {
      // Arrange.
      const moduleElement = await initializeModule(
          createSampleCluster(undefined, MIN_RELATED_SEARCHES - 1));

      // Assert.
      assertEquals(null, moduleElement);
      assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
      assertEquals(
          1,
          metrics.count(
              DISPLAY_LAYOUT_METRIC_NAME, HistoryClusterLayoutType.NONE));
    });

    test('Layout 1 is used', async () => {
      // Arrange.
      const moduleElement = await initializeModule(createSampleCluster());

      // Assert.
      assertTrue(!!moduleElement);
      assertLayoutSet(moduleElement, HistoryClusterLayoutType.LAYOUT_1);
      // Check that metrics are set.
      assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
      assertEquals(
          1,
          metrics.count(
              DISPLAY_LAYOUT_METRIC_NAME, HistoryClusterLayoutType.LAYOUT_1));
      // Check that the visits are processed and set properly.
      const visits = moduleElement.cluster.visits;
      assertEquals(visits.length, LAYOUT_1_MIN_VISITS);
      for (let i = 0; i < visits.length; i++) {
        assertTrue(!!visits[i]);
        if (i < LAYOUT_1_MIN_IMAGE_VISITS) {
          assertTrue(visits[i]!.hasUrlKeyedImage);
        }
      }
    });

    test('Layout 2 is used', async () => {
      // Arrange.
      const moduleElement = await initializeModule(
          createSampleCluster(HistoryClusterLayoutType.LAYOUT_2));

      // Assert.
      assertTrue(!!moduleElement);
      assertLayoutSet(moduleElement, HistoryClusterLayoutType.LAYOUT_2);
      // Check that metrics are set.
      assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
      assertEquals(
          1,
          metrics.count(
              DISPLAY_LAYOUT_METRIC_NAME, HistoryClusterLayoutType.LAYOUT_2));
      // Check that the visits are processed and set properly.
      const visits = moduleElement.cluster.visits;
      assertEquals(visits.length, LAYOUT_2_MIN_VISITS);
      for (let i = 0; i < visits.length; i++) {
        assertTrue(!!visits[i]);
        if (i < LAYOUT_2_MIN_IMAGE_VISITS) {
          assertTrue(visits[i]!.hasUrlKeyedImage);
        }
      }
    });

    test('Layout 3 is used', async () => {
      // Arrange.
      const moduleElement = await initializeModule(
          createSampleCluster(HistoryClusterLayoutType.LAYOUT_3));

      // Assert.
      assertTrue(!!moduleElement);
      assertLayoutSet(moduleElement, HistoryClusterLayoutType.LAYOUT_3);
      // Check that metrics are set.
      assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
      assertEquals(
          1,
          metrics.count(
              DISPLAY_LAYOUT_METRIC_NAME, HistoryClusterLayoutType.LAYOUT_3));
      // Check that the visits are processed and set properly.
      const visits = moduleElement.cluster.visits;
      assertEquals(visits.length, LAYOUT_3_MIN_VISITS);
      for (let i = 0; i < visits.length; i++) {
        assertTrue(!!visits[i]);
        if (i < LAYOUT_3_MIN_IMAGE_VISITS) {
          assertTrue(visits[i]!.hasUrlKeyedImage);
        }
      }
    });

    test('Header element populated with correct data', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElement = await initializeModule(createSampleCluster(
          undefined, undefined, {label: sampleClusterLabel}));

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);

      assertEquals(
          'Show all',
          headerElement.querySelector('#showAllButton')!.innerHTML.trim());
      assertModuleHeaderTitle(
          headerElement, `Resume your journey for ${sampleClusterLabel}`);
    });

    test('Header info button click opens info dialog', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElement = await initializeModule(createSampleCluster(
          undefined, undefined, {label: sampleClusterLabel}));

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);

      headerElement!.dispatchEvent(new Event('info-button-click'));

      // Assert.
      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });

    test('Backend is notified when Show all button is triggered', async () => {
      const sampleClusterUnquotedLabel = 'Sample Journey';
      const sampleClusterLabel = `"${sampleClusterUnquotedLabel}"`;
      const moduleElement = await initializeModule(createSampleCluster(
          undefined, MIN_RELATED_SEARCHES, {label: sampleClusterLabel}));
      assertTrue(!!moduleElement);

      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);
      const showAllButton =
          headerElement.querySelector('#showAllButton') as HTMLElement;
      assertTrue(!!showAllButton);

      showAllButton.click();
      const query = await handler.whenCalled('showJourneysSidePanel');
      assertEquals(sampleClusterUnquotedLabel, query);
      assertEquals(
          1,
          metrics.count(`NewTabPage.HistoryClusters.Layout${
              HistoryClusterLayoutType.LAYOUT_1}.Click`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${
                  HistoryClusterLayoutType.LAYOUT_1}.Click`,
              HistoryClusterElementType.SHOW_ALL));
    });

    test(
        'Backend is notified when "Open all in tab group" is triggered',
        async () => {
          const sampleCluster =
              createSampleCluster(HistoryClusterLayoutType.LAYOUT_1);
          const moduleElement = await initializeModule(sampleCluster);
          assertTrue(!!moduleElement);

          const openAllButton =
              moduleElement.shadowRoot!.querySelector('ntp-module-header')!
                  .querySelector<HTMLElement>('#openAllInTabGroupButton')!;
          assertEquals(
              (loadTimeData.getString(
                  'modulesJourneysOpenAllInNewTabGroupButtonText')),
              openAllButton.innerText.trim());
          openAllButton.click();

          const urls = await handler.whenCalled('openUrlsInTabGroup');
          assertEquals(3, urls.length);
          assertEquals(`${GOOGLE_SEARCH_BASE_URL}?q=foo`, urls[0].url);
          assertEquals('https://www.foo.com/1', urls[1].url);
          assertEquals('https://www.foo.com/2', urls[2].url);
        });

    test('Backend is notified when module is dismissed', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const sampleCluster = createSampleCluster(
          undefined, undefined, {label: sampleClusterLabel});
      const moduleElement = await initializeModule(sampleCluster);
      assertTrue(!!moduleElement);

      // Act.
      const waitForDismissEvent =
          eventToPromise('dismiss-module', moduleElement);
      const dismissButton =
          moduleElement.shadowRoot!.querySelector('ntp-module-header')!
              .shadowRoot!.querySelector<HTMLElement>('#dismissButton')!;
      dismissButton.click();

      // Assert.
      const dismissEvent: DismissModuleEvent = await waitForDismissEvent;
      assertEquals(
          `${sampleCluster.label!} hidden`, dismissEvent.detail.message);
      const visits = await handler.whenCalled('dismissCluster');
      assertEquals(3, visits.length);
      visits.forEach((visit: URLVisit, index: number) => {
        assertEquals(index, Number(visit.visitId));
      });
    });

    [HistoryClusterLayoutType.LAYOUT_1, HistoryClusterLayoutType.LAYOUT_2,
     HistoryClusterLayoutType.LAYOUT_3]
        .forEach(layoutType => {
          test('Module produces visit tile click metrics', async () => {
            // Arrange.
            const moduleElement =
                await initializeModule(createSampleCluster(layoutType));

            // Assert.
            assertTrue(!!moduleElement);
            const tileElement =
                $$(moduleElement, 'ntp-history-clusters-tile') as HTMLElement;
            assertTrue(!!tileElement);

            ($$(tileElement, '#content') as HTMLElement).click();
            assertEquals(
                1,
                metrics.count(`NewTabPage.HistoryClusters.Layout${
                    layoutType}.VisitTile.ClickIndex`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`,
                    HistoryClusterElementType.VISIT));
          });

          test('Module produces suggest tile click metrics', async () => {
            // Arrange.
            const moduleElement =
                await initializeModule(createSampleCluster(layoutType));

            // Assert.
            assertTrue(!!moduleElement);
            const suggestTileElement =
                $$(moduleElement, 'ntp-history-clusters-suggest-tile');
            assertTrue(!!suggestTileElement);

            ($$(suggestTileElement, '.related-search') as HTMLElement).click();
            assertEquals(
                1,
                metrics.count(`NewTabPage.HistoryClusters.Layout${
                    layoutType}.SuggestTile.ClickIndex`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`,
                    HistoryClusterElementType.SUGGEST));
          });
        });
  });

  suite('unload metric no images', () => {
    test('Module records no images state metric on unload', async () => {
      imageServiceHandler.setResultFor(
          'getPageImageUrl', Promise.resolve(null));

      const moduleElement = await initializeModule(
          createSampleCluster(HistoryClusterLayoutType.LAYOUT_1));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);

      window.dispatchEvent(new Event('unload'));

      assertEquals(2, imageServiceHandler.getCallCount('getPageImageUrl'));
      assertEquals(
          1,
          metrics.count(`NewTabPage.HistoryClusters.Layout${
              HistoryClusterLayoutType.LAYOUT_1}.ImageDisplayState`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${
                  HistoryClusterLayoutType.LAYOUT_1}.ImageDisplayState`,
              HistoryClusterImageDisplayState.NONE));
    });
  });

  suite('unload metric all images', () => {
    test('Module records all images state metric on unload', async () => {
      imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
        result: {imageUrl: {url: 'https://example.com/image.png'}},
      }));

      const moduleElement = await initializeModule(
          createSampleCluster(HistoryClusterLayoutType.LAYOUT_1));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);

      window.dispatchEvent(new Event('unload'));

      assertEquals(2, imageServiceHandler.getCallCount('getPageImageUrl'));
      assertEquals(
          1,
          metrics.count(`NewTabPage.HistoryClusters.Layout${
              HistoryClusterLayoutType.LAYOUT_1}.ImageDisplayState`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${
                  HistoryClusterLayoutType.LAYOUT_1}.ImageDisplayState`,
              HistoryClusterImageDisplayState.ALL));
    });
  });
});
