// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-breadcrumb component.  */

import {Paths} from 'chrome://personalization/trusted/personalization_router_element.js';
import {WallpaperBreadcrumb} from 'chrome://personalization/trusted/wallpaper_breadcrumb_element.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';
import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperBreadcrumbTest() {
  /** @type {?HTMLElement} */
  let wallpaperBreadcrumbElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (wallpaperBreadcrumbElement) {
      wallpaperBreadcrumbElement.remove();
    }
    wallpaperBreadcrumbElement = null;
    await flushTasks();
  });

  test('shows wallpaper label by default', async () => {
    wallpaperBreadcrumbElement = initElement(WallpaperBreadcrumb.is);

    const p =
        wallpaperBreadcrumbElement.shadowRoot.getElementById('wallpaperLabel');
    assertEquals(wallpaperBreadcrumbElement.i18n('title'), p.textContent);
  });

  test('does not create page label by default', async () => {
    wallpaperBreadcrumbElement = initElement(WallpaperBreadcrumb.is);

    const pageLabel =
        wallpaperBreadcrumbElement.shadowRoot.getElementById('pageLabel');
    assertFalse(!!pageLabel);
  });

  test('shows collection name when collection is selected', async () => {
    const collection = wallpaperProvider.collections[0];
    wallpaperBreadcrumbElement = initElement(
        WallpaperBreadcrumb.is,
        {'path': Paths.CollectionImages, 'collectionId': collection.id});

    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperBreadcrumbElement);

    const label =
        wallpaperBreadcrumbElement.shadowRoot.getElementById('pageLabel');
    assertTrue(!!label && !label.hidden);
    assertEquals(collection.name, label.textContent.trim());
  });

  test('show label when local images subpage is loaded', async () => {
    wallpaperBreadcrumbElement =
        initElement(WallpaperBreadcrumb.is, {'path': Paths.LocalCollection});

    personalizationStore.data.local.images = wallpaperProvider.localImages;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperBreadcrumbElement);

    const label =
        wallpaperBreadcrumbElement.shadowRoot.getElementById('pageLabel');
    assertTrue(!!label && !label.hidden);
    assertEquals(
        wallpaperBreadcrumbElement.i18n('myImagesLabel'),
        label.textContent.trim());
  });
}
