// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-breadcrumb component.  */

import {WallpaperCollection} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {Paths} from 'chrome://personalization/trusted/personalization_router_element.js';
import {WallpaperBreadcrumb} from 'chrome://personalization/trusted/wallpaper_breadcrumb_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

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

  /**
   * Asserts that the specified |breadcrumbContainer| has children conforming
   * to the specified |breadcrumbs|, e.g. ["Wallpaper", "Google Photos"].
   * @param {HTMLElement} breadcrumbContainer
   * @param {Array<string>} breadcrumbs
   */
  function assertBreadcrumbs(breadcrumbContainer, breadcrumbs) {
    // Ignore child elements which are not breadcrumbs.
    const breadcrumbEls = [...breadcrumbContainer.children].filter(child => {
      return child.classList.contains('breadcrumb');
    });

    assertEquals(breadcrumbEls.length, breadcrumbs.length);

    for (let i = 0; i < breadcrumbs.length; ++i) {
      const breadcrumb = breadcrumbs[i];
      const breadcrumbEl = breadcrumbEls[i];

      assertEquals(breadcrumbEl.textContent, breadcrumb);

      if (i < breadcrumbs.length - 1) {
        // Breadcrumbs are separated by a chevron icon.
        const chevronEl = breadcrumbEl.nextElementSibling;
        assertEquals(chevronEl.tagName, 'IRON-ICON');
        assertEquals(chevronEl.getAttribute('icon'), 'cr:chevron-right');
      }
    }
  }

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

    const breadcrumbContainer =
        wallpaperBreadcrumbElement.shadowRoot.getElementById(
            'breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer, [wallpaperBreadcrumbElement.i18n('title')]);
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

    const breadcrumbContainer =
        wallpaperBreadcrumbElement.shadowRoot.getElementById(
            'breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer,
        [wallpaperBreadcrumbElement.i18n('title'), collection.name]);
  });

  test('show album name when Google Photos subpage is loaded', async () => {
    // The `googlePhotosLabel` string is only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({'googlePhotosLabel': 'Google Photos'});

    const googlePhotosAlbum = new WallpaperCollection();
    googlePhotosAlbum.id = '9bd1d7a3-f995-4445-be47-53c5b58ce1cb';
    googlePhotosAlbum.name = 'Album 0';

    personalizationStore.data.googlePhotos.albums = [googlePhotosAlbum];
    personalizationStore.notifyObservers();

    wallpaperBreadcrumbElement = initElement(WallpaperBreadcrumb.is, {
      'path': Paths.GooglePhotosCollection,
      'googlePhotosAlbumId': googlePhotosAlbum.id
    });

    const breadcrumbContainer =
        wallpaperBreadcrumbElement.shadowRoot.getElementById(
            'breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      wallpaperBreadcrumbElement.i18n('title'),
      wallpaperBreadcrumbElement.i18n('googlePhotosLabel'),
      googlePhotosAlbum.name
    ]);
  });

  test('show label when Google Photos subpage is loaded', async () => {
    // The `googlePhotosLabel` string is only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({'googlePhotosLabel': 'Google Photos'});

    wallpaperBreadcrumbElement = initElement(
        WallpaperBreadcrumb.is, {'path': Paths.GooglePhotosCollection});

    const breadcrumbContainer =
        wallpaperBreadcrumbElement.shadowRoot.getElementById(
            'breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      wallpaperBreadcrumbElement.i18n('title'),
      wallpaperBreadcrumbElement.i18n('googlePhotosLabel')
    ]);
  });

  test('show label when local images subpage is loaded', async () => {
    wallpaperBreadcrumbElement =
        initElement(WallpaperBreadcrumb.is, {'path': Paths.LocalCollection});

    personalizationStore.data.local.images = wallpaperProvider.localImages;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperBreadcrumbElement);

    const breadcrumbContainer =
        wallpaperBreadcrumbElement.shadowRoot.getElementById(
            'breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      wallpaperBreadcrumbElement.i18n('title'),
      wallpaperBreadcrumbElement.i18n('myImagesLabel')
    ]);
  });
}
