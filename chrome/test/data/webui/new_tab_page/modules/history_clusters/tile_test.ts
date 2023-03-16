// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {TileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createVisit} from './test_support.js';

suite('NewTabPageModulesHistoryClustersModuleTileTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  async function initializeModule(visit: URLVisit): Promise<TileModuleElement> {
    const tileElement = new TileModuleElement();
    tileElement.visit = visit;
    document.body.append(tileElement);
    await waitAfterNextRender(tileElement);
    return tileElement;
  }

  test('Tile element populated with correct data', async () => {
    // Arrange.
    const tileElement = await initializeModule(createVisit(
        BigInt(1), 'https://www.test.com/1', 'https://www.test.com/1',
        'Test Title 1', false, '1 min ago'));

    // Assert.
    assertTrue(!!tileElement);
    assertEquals($$(tileElement, '#title')!.innerHTML, 'Test Title 1');
    assertEquals('1 min ago', $$(tileElement, '#date')!.innerHTML);
    assertTrue(
        !!window.getComputedStyle($$<HTMLImageElement>(tileElement, '#icon')!)
              .getPropertyValue('background-image')
              .trim());
  });
});
