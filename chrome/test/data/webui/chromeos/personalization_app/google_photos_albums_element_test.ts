// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GooglePhotosAlbums} from 'chrome://personalization/trusted/wallpaper/google_photos_albums_element.js';

import {initializeGooglePhotosData} from 'chrome://personalization/trusted/wallpaper/wallpaper_controller.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function GooglePhotosAlbumsTest() {
  let googlePhotosAlbumsElement: GooglePhotosAlbums|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns all matches for |selector| in |googlePhotosAlbumsElement|'s shadow
   * DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosAlbumsElement!.shadowRoot!.querySelectorAll(selector);
    return matches ? [...matches] : null;
  }

  setup(() => {
    loadTimeData.overrideValues({'isGooglePhotosIntegrationEnabled': true});

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosAlbumsElement);
    googlePhotosAlbumsElement = null;
  });

  test('displays albums', async () => {
    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosCount(2);
    wallpaperProvider.setGooglePhotosAlbums([
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        title: 'Album 0',
        photoCount: 1,
        preview: {url: '...'}
      },
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        title: 'Album 1',
        photoCount: 2,
        preview: {url: '...'}
      }
    ]);

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbums, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The |personalizationStore| should be empty, so no albums should be
    // rendered initially.
    const albumSelector = 'wallpaper-grid-item:not([hidden]) .album';
    assertEquals(querySelectorAll(albumSelector)!.length, 0);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Verify that the expected |albums| are rendered.
    const albums = querySelectorAll(albumSelector);
    assertEquals(albums!.length, 2);
    assertEquals(albums![0]!.innerHTML.trim(), 'Album 0');
    assertEquals(albums![1]!.innerHTML.trim(), 'Album 1');
  });
}
