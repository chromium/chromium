// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Cluster, SearchQuery, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters.mojom-webui.js';
import {DismissModuleEvent, HistoryClusterLayoutType, historyClustersDescriptor, HistoryClustersModuleElement, HistoryClustersProxyImpl, LAYOUT_1_MIN_IMAGE_VISITS, LAYOUT_1_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS, LAYOUT_2_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS, LAYOUT_3_MIN_VISITS, MIN_RELATED_SEARCHES} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

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

function createVisit(
    visitId: bigint, normalizedUrl: string, urlForDisplay: string,
    pageTitle: string, hasUrlKeyedImage: boolean): URLVisit {
  return {
    visitId: visitId,
    normalizedUrl: {url: normalizedUrl},
    urlForDisplay: urlForDisplay,
    pageTitle: pageTitle,
    titleMatchPositions: [],
    urlForDisplayMatchPositions: [],
    duplicates: [],
    relativeDate: '',
    annotations: [],
    debugInfo: {},
    rawVisitData: {
      url: {url: ''},
      visitTime: {internalValue: BigInt(0)},
    },
    hasUrlKeyedImage: hasUrlKeyedImage,
    isKnownToSync: false,
  };
}

// Use Layout 1 as default for tests that do not care which layout.
function createSampleVisits(
    numVisits: number = LAYOUT_1_MIN_VISITS,
    numImageVisits: number = LAYOUT_1_MIN_IMAGE_VISITS): URLVisit[] {
  const result: URLVisit[] = [];

  // Create SRP visit.
  result.push(createVisit(
      BigInt(0), 'https://www.google.com/', 'www.google.com', 'SRP', false));

  // Create general visits.
  for (let i = 1; i <= numVisits; i++) {
    result.push(createVisit(
        BigInt(i), `https://www.foo.com/${i}`, `www.foo.com/${i}`,
        `Test Title ${i}`, i <= numImageVisits));
  }
  return result;
}

function createRelatedSearches(num: number = MIN_RELATED_SEARCHES):
    SearchQuery[] {
  const result: SearchQuery[] = [];

  for (let i = 0; i < num; i++) {
    result.push({
      query: `Test Query ${i}`,
      url: {
        url:
            `https://www.google.com/search?q=${encodeURIComponent(`test${i}`)}`,
      },
    });
  }
  return result;
}

function createLayoutSuitableSampleVisits(
    layoutType: HistoryClusterLayoutType) {
  switch (layoutType) {
    case HistoryClusterLayoutType.LAYOUT_1:
      return createSampleVisits(LAYOUT_1_MIN_VISITS, LAYOUT_1_MIN_IMAGE_VISITS);
    case HistoryClusterLayoutType.LAYOUT_2:
      return createSampleVisits(LAYOUT_2_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS);
    case HistoryClusterLayoutType.LAYOUT_3:
      return createSampleVisits(LAYOUT_3_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS);
  }

  throw Error();
}

function createSampleCluster(overrides?: Partial<Cluster>): Cluster {
  const cluster: Cluster = Object.assign(
      {
        id: BigInt(111),
        visits: createSampleVisits(),
        label: undefined,
        labelMatchPositions: [],
        relatedSearches: createRelatedSearches(),
        imageUrl: undefined,
        fromPersistence: false,
        debugInfo: undefined,
      },
      overrides);

  return cluster;
}

suite('NewTabPageModulesHistoryClustersModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImpl.setInstance(
            new HistoryClustersProxyImpl(mock)));
    metrics = fakeMetricsPrivate();
  });

  test('No module created if no history cluster data', async () => {
    // Arrange.
    handler.setResultFor('getCluster', Promise.resolve({cluster: null}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('getCluster');
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
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(cluster)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('getCluster');
    assertEquals(null, moduleElement);
  });

  test('No module created when less than min related searches', async () => {
    // Arrange.
    const cluster: Partial<Cluster> = {
      relatedSearches: createRelatedSearches(MIN_RELATED_SEARCHES - 1),
    };
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(cluster)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    // Assert.
    await handler.whenCalled('getCluster');
    assertEquals(null, moduleElement);
    assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
    assertEquals(
        1,
        metrics.count(
            DISPLAY_LAYOUT_METRIC_NAME, HistoryClusterLayoutType.NONE));
  });

  test('Layout 1 is used', async () => {
    // Arrange.
    // Layout 1 has the same min image and min total.
    const cluster: Partial<Cluster> = {
      visits: createSampleVisits(
          LAYOUT_1_MIN_VISITS + 1, LAYOUT_1_MIN_IMAGE_VISITS + 1),
    };
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(cluster)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);

    // Assert.
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
    const cluster: Partial<Cluster> = {
      visits: createSampleVisits(
          LAYOUT_2_MIN_VISITS + 3, LAYOUT_2_MIN_IMAGE_VISITS),
    };
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(cluster)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);

    // Assert.
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
    const cluster: Partial<Cluster> = {
      visits: createSampleVisits(
          LAYOUT_3_MIN_VISITS + 2, LAYOUT_3_MIN_IMAGE_VISITS + 2),
    };
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster(cluster)}));

    // Act.
    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);

    // Assert.
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
    assertEquals($$(tileElement, '#title')!.innerHTML, 'Test Title 1');
  });

  test('Related searches element populated with correct data', async () => {
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster()}));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    document.body.append(moduleElement);
    assertTrue(!!moduleElement);

    await handler.whenCalled('getCluster');
    await waitAfterNextRender(moduleElement);

    const suggestTileElement =
        $$(moduleElement, 'ntp-history-clusters-suggest-tile');
    assertTrue(!!suggestTileElement);

    assertEquals($$(suggestTileElement, '.title')!.innerHTML, 'Test Query 0');

    assertEquals(
        suggestTileElement.shadowRoot!.querySelectorAll('.title').length, 3);
  });

  test('Header element populated with correct data', async () => {
    const sampleClusterLabel = '"Sample Journey"';
    handler.setResultFor('getCluster', Promise.resolve({
      cluster: createSampleCluster({label: sampleClusterLabel}),
    }));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    document.body.append(moduleElement);
    assertTrue(!!moduleElement);
    await handler.whenCalled('getCluster');
    await waitAfterNextRender(moduleElement);

    const headerElement = $$(moduleElement, 'ntp-module-header');
    assertTrue(!!headerElement);

    assertEquals(
        'Show all',
        headerElement.querySelector('#showAllButton')!.innerHTML.trim());
    assertModuleHeaderTitle(
        headerElement, `Resume your journey for ${sampleClusterLabel}`);
  });

  test('Header title falls back to Visit title', async () => {
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: createSampleCluster()}));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    document.body.append(moduleElement);
    assertTrue(!!moduleElement);
    await handler.whenCalled('getCluster');
    await waitAfterNextRender(moduleElement);

    const headerElement = $$(moduleElement, 'ntp-module-header');
    assertTrue(!!headerElement);
    assertModuleHeaderTitle(headerElement, 'Resume your journey for SRP');
  });

  test('Backend is notified when module is dismissed', async () => {
    const sampleClusterLabel = '"Sample Journey"';
    const sampleCluster = createSampleCluster({label: sampleClusterLabel});
    handler.setResultFor(
        'getCluster', Promise.resolve({cluster: sampleCluster}));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;

    document.body.append(moduleElement);
    assertTrue(!!moduleElement);
    await handler.whenCalled('getCluster');
    await waitAfterNextRender(moduleElement);

    const waitForDismissEvent = eventToPromise('dismiss-module', moduleElement);
    const dismissButton =
        moduleElement.shadowRoot!.querySelector('ntp-module-header')!
            .shadowRoot!.querySelector<HTMLElement>('#dismissButton')!;
    dismissButton.click();
    const dismissEvent: DismissModuleEvent = await waitForDismissEvent;
    assertEquals(`${sampleCluster.label!} hidden`, dismissEvent.detail.message);
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
          handler.setResultFor('getCluster', Promise.resolve({
            cluster: createSampleCluster(
                {visits: createLayoutSuitableSampleVisits(layoutType)}),
          }));

          const moduleElement = await historyClustersDescriptor.initialize(0) as
              HistoryClustersModuleElement;

          document.body.append(moduleElement);
          assertTrue(!!moduleElement);
          await handler.whenCalled('getCluster');
          await waitAfterNextRender(moduleElement);

          const tileElement =
              $$(moduleElement, 'ntp-history-clusters-tile') as HTMLElement;
          assertTrue(!!tileElement);

          ($$(tileElement, '#content') as HTMLElement).click();
          assertEquals(
              1,
              metrics.count(`NewTabPage.HistoryClusters.Layout${
                  layoutType}.VisitTile.ClickIndex`));
        });

        test('Module produces suggest tile click metrics', async () => {
          handler.setResultFor('getCluster', Promise.resolve({
            cluster: createSampleCluster(
                {visits: createLayoutSuitableSampleVisits(layoutType)}),
          }));

          const moduleElement = await historyClustersDescriptor.initialize(0) as
              HistoryClustersModuleElement;

          document.body.append(moduleElement);
          assertTrue(!!moduleElement);
          await handler.whenCalled('getCluster');
          await waitAfterNextRender(moduleElement);

          const suggestTileElement =
              $$(moduleElement, 'ntp-history-clusters-suggest-tile');
          assertTrue(!!suggestTileElement);

          ($$(suggestTileElement, '.related-search') as HTMLElement).click();
          assertEquals(
              1,
              metrics.count(`NewTabPage.HistoryClusters.Layout${
                  layoutType}.SuggestTile.ClickIndex`));
        });
      });
});
