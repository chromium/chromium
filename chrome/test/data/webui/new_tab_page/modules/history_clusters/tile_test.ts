// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Annotation, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageImageServiceBrowserProxy, TileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ClientId as PageImageServiceClientId, PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {assertStyle, installMock} from '../../test_support.js';

import {createVisit} from './test_support.js';

suite('NewTabPageModulesHistoryClustersModuleTileTest', () => {
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    imageServiceHandler = installMock(
        PageImageServiceHandlerRemote,
        mock => PageImageServiceBrowserProxy.setInstance(
            new PageImageServiceBrowserProxy(mock)));
    metrics = fakeMetricsPrivate();
  });

  function initializeModule(
      visit: URLVisit, smallFormat: boolean = false): TileModuleElement {
    const tileElement = new TileModuleElement();
    tileElement.smallFormat = smallFormat;
    tileElement.visit = visit;
    document.body.append(tileElement);
    return tileElement;
  }

  test('Tile element populated with correct data', async () => {
    // Arrange.
    const tileElement = initializeModule(createVisit({
      relativeDate: '1 min ago',
      annotations: [Annotation.kBookmarked],
    }));

    // Assert.
    await waitAfterNextRender(tileElement);
    assertTrue(!!tileElement);
    assertEquals($$(tileElement, '#title')!.innerHTML, 'Test Title');
    assertEquals('1 min ago', $$(tileElement, '#date')!.innerHTML);
    assertTrue(
        !!window.getComputedStyle($$<HTMLImageElement>(tileElement, '#icon')!)
              .getPropertyValue('background-image')
              .trim());
    assertEquals('Bookmarked', $$(tileElement, '#annotation')!.innerHTML);
  });

  [true, false].forEach(
      (success) =>
          test(`Metric sent on success ${success} image load`, async () => {
            // Arrange.
            const imageResult = success ?
                {result: {imageUrl: {url: 'https://example.com/image.png'}}} :
                null;
            imageServiceHandler.setResultFor(
                'getPageImageUrl', Promise.resolve(imageResult));
            initializeModule(createVisit({
              hasUrlKeyedImage: true,
              relativeDate: '1 min ago',
            }));

            await flushTasks();

            // Assert
            assertEquals(
                1, imageServiceHandler.getCallCount('getPageImageUrl'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.HistoryClusters.ImageLoadSuccess', success));
          }));

  test('Tile shows background image if exists', async () => {
    // Set result for getPageImageUrl.
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: {imageUrl: {url: 'https://example.com/image.png'}},
    }));
    const visit = createVisit({hasUrlKeyedImage: true});
    const tileElement = initializeModule(visit);

    // Assert.
    const [clientId, pageUrl] =
        await imageServiceHandler.whenCalled('getPageImageUrl');
    assertEquals(PageImageServiceClientId.NtpQuests, clientId);
    assertEquals(visit.normalizedUrl, pageUrl);

    await flushTasks();
    assertTrue(!!$$(tileElement, '#image img'));
    assertTrue(!$$(tileElement, '#image page-favicon'));
  });

  test('Tile does not call for or display image if small format', async () => {
    // Set result for getPageImageUrl.
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: {imageUrl: {url: 'https://example.com/image.png'}},
    }));
    const visit = createVisit({hasUrlKeyedImage: true});
    const tileElement = initializeModule(visit, true);

    // Assert.
    await flushTasks();
    assertEquals(0, imageServiceHandler.getCallCount('getPageImageUrl'));
    assertStyle($$(tileElement, '#image')!, 'display', 'none');
  });

  test('Tile shows favicon if no image', async () => {
    // Set result for getPageImageUrl.
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: null,
    }));
    const visit = createVisit({hasUrlKeyedImage: true});
    const tileElement = initializeModule(visit);

    // Assert.
    await flushTasks();
    assertEquals(1, imageServiceHandler.getCallCount('getPageImageUrl'));
    assertTrue(!$$(tileElement, '#image img'));
    assertTrue(!!$$(tileElement, '#image page-favicon'));
  });

  test('Tile shows and hides discount chip', async () => {
    // Arrange.
    const tileElement = initializeModule(createVisit({
      relativeDate: '1 min ago',
      annotations: [Annotation.kBookmarked],
    }));

    // Assert.
    await waitAfterNextRender(tileElement);
    assertTrue(!!tileElement);
    assertTrue(!$$(tileElement, '#discountChip'));

    // Act.
    tileElement.discount = '15% off';

    // Assert.
    await waitAfterNextRender(tileElement);
    assertTrue(!!tileElement);
    assertTrue(!!$$(tileElement, '#discountChip'));
    assertEquals('15% off', $$(tileElement, '#discountChip')!.textContent);
  });
});
