// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Annotation, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {ImageServiceBrowserProxy, TileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ImageServiceHandlerRemote} from 'chrome://resources/cr_components/image_service/image_service.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../../test_support.js';

import {createVisit} from './test_support.js';

suite('NewTabPageModulesHistoryClustersModuleTileTest', () => {
  let imageServiceHandler: TestMock<ImageServiceHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    imageServiceHandler = installMock(
        ImageServiceHandlerRemote,
        mock => ImageServiceBrowserProxy.setInstance(
            new ImageServiceBrowserProxy(mock)));
    metrics = fakeMetricsPrivate();
  });

  function initializeModule(visit: URLVisit): TileModuleElement {
    const tileElement = new TileModuleElement();
    tileElement.visit = visit;
    document.body.append(tileElement);
    return tileElement;
  }

  test('Tile element populated with correct data', async () => {
    // Arrange.
    const tileElement = initializeModule(createVisit(
        BigInt(1), 'https://www.test.com/1', 'https://www.test.com/1',
        'Test Title 1', false, '1 min ago', [Annotation.kBookmarked]));

    // Assert.
    await waitAfterNextRender(tileElement);
    assertTrue(!!tileElement);
    assertEquals($$(tileElement, '#title')!.innerHTML, 'Test Title 1');
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
            initializeModule(createVisit(
                BigInt(1), 'https://www.test.com/1', 'https://www.test.com/1',
                'Test Title 1', true, '1 min ago'));

            await flushTasks();

            // Assert
            assertEquals(
                1, imageServiceHandler.getCallCount('getPageImageUrl'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.HistoryClusters.ImageLoadSuccess', success));
          }));
});
