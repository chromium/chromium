// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-breadcrumb component.  */

import {WallpaperCollection} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {PersonalizationBreadcrumb} from 'chrome://personalization/trusted/personalization_breadcrumb_element.js';
import {Paths} from 'chrome://personalization/trusted/personalization_router_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function PersonalizationBreadcrumbTest() {
  /** @type {?HTMLElement} */
  let breadcrumbElement = null;

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
    if (breadcrumbElement) {
      breadcrumbElement.remove();
    }
    breadcrumbElement = null;
    await flushTasks();
  });

  test('shows wallpaper label by default', async () => {
    breadcrumbElement = initElement(PersonalizationBreadcrumb.is);

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot.getElementById('breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [breadcrumbElement.i18n('title')]);
  });

  test('shows collection name when collection is selected', async () => {
    const collection = wallpaperProvider.collections[0];
    breadcrumbElement = initElement(
        PersonalizationBreadcrumb.is,
        {'path': Paths.CollectionImages, 'collectionId': collection.id});

    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(breadcrumbElement);

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot.getElementById('breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer,
        [breadcrumbElement.i18n('title'), collection.name]);
  });

  test('show album name when Google Photos subpage is loaded', async () => {
    // The `googlePhotosLabel` string is only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({'googlePhotosLabel': 'Google Photos'});

    const googlePhotosAlbum = new WallpaperCollection();
    googlePhotosAlbum.id = '9bd1d7a3-f995-4445-be47-53c5b58ce1cb';
    googlePhotosAlbum.name = 'Album 0';

    personalizationStore.data.wallpaper.googlePhotos.albums =
        [googlePhotosAlbum];
    personalizationStore.notifyObservers();

    breadcrumbElement = initElement(PersonalizationBreadcrumb.is, {
      'path': Paths.GooglePhotosCollection,
      'googlePhotosAlbumId': googlePhotosAlbum.id
    });

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot.getElementById('breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('title'),
      breadcrumbElement.i18n('googlePhotosLabel'), googlePhotosAlbum.name
    ]);
  });

  test('show label when Google Photos subpage is loaded', async () => {
    // The `googlePhotosLabel` string is only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({'googlePhotosLabel': 'Google Photos'});

    breadcrumbElement = initElement(
        PersonalizationBreadcrumb.is, {'path': Paths.GooglePhotosCollection});

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot.getElementById('breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('title'),
      breadcrumbElement.i18n('googlePhotosLabel')
    ]);
  });

  test('show label when local images subpage is loaded', async () => {
    breadcrumbElement = initElement(
        PersonalizationBreadcrumb.is, {'path': Paths.LocalCollection});

    personalizationStore.data.wallpaper.local.images =
        wallpaperProvider.localImages;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(breadcrumbElement);

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot.getElementById('breadcrumbContainer');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('title'), breadcrumbElement.i18n('myImagesLabel')
    ]);
  });
}
