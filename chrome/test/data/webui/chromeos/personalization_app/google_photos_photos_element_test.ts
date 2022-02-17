// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GooglePhotosPhoto, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {GooglePhotosPhotos} from 'chrome://personalization/trusted/wallpaper/google_photos_photos_element.js';
import {initializeGooglePhotosData} from 'chrome://personalization/trusted/wallpaper/wallpaper_controller.js';
import {WallpaperGridItem} from 'chrome://personalization/trusted/wallpaper/wallpaper_grid_item_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function GooglePhotosPhotosTest() {
  let googlePhotosPhotosElement: GooglePhotosPhotos|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns all matches for |selector| in |googlePhotosPhotosElement|'s shadow
   * DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosPhotosElement!.shadowRoot!.querySelectorAll(selector);
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
    await teardownElement(googlePhotosPhotosElement);
    googlePhotosPhotosElement = null;
  });

  test('displays photos', async () => {
    const photos: GooglePhotosPhoto[] = [
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        name: 'foo',
        date: {data: []},
        url: {url: 'foo.com'}
      },
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        name: 'bar',
        date: {data: []},
        url: {url: 'bar.com'}
      },
      {
        id: '0a268a37-877a-4936-81d4-38cc84b0f596',
        name: 'baz',
        date: {data: []},
        url: {url: 'baz.com'}
      }
    ];

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(photos);
    wallpaperProvider.setGooglePhotosCount(photos.length);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // The |personalizationStore| should be empty, so no photos should be
    // rendered initially.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected |photos| are rendered.
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, photos.length);
    photoEls.forEach((photoEl, i) => {
      assertEquals(photoEl.imageSrc, photos[i]!.url.url);
      assertEquals(photoEl.primaryText, undefined);
      assertEquals(photoEl.secondaryText, undefined);
    });
  });

  test('displays photo selected', async () => {
    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'}
    };

    const anotherPhoto: GooglePhotosPhoto = {
      id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
      name: 'bar',
      date: {data: []},
      url: {url: 'bar.com'}
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos([photo, anotherPhoto]);
    wallpaperProvider.setGooglePhotosCount(2);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected photos are rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 2);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |photo|.
    personalizationStore.data.wallpaper.pendingSelected = photo;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: photo.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kGooglePhotos,
      key: photo.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |anotherPhoto|.
    personalizationStore.data.wallpaper.pendingSelected = anotherPhoto;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: anotherPhoto.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kGooglePhotos,
      key: anotherPhoto.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Start a pending selection for a |FilePath| backed wallpaper.
    personalizationStore.data.wallpaper.pendingSelected = {path: '//foo'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'foo://'},
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kCustomized,
      key: '//foo'
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);
  });

  test('selects photo', async () => {
    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'}
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos([photo]);
    wallpaperProvider.setGooglePhotosCount(1);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected |photo| is rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 1);
    assertEquals(photoEls[0]!.imageSrc, photo.url.url);
    assertEquals(photoEls[0]!.primaryText, undefined);
    assertEquals(photoEls[0]!.secondaryText, undefined);

    // Select |photo| and verify selection started.
    photoEls[0]!.click();
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 1);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, true);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, photo);

    // Wait for and verify hard-coded selection failure.
    const methodName = 'selectGooglePhotosPhoto';
    assertEquals(await wallpaperProvider.whenCalled(methodName), photo.id);
    await waitAfterNextRender(googlePhotosPhotosElement);
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 0);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, false);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, null);
  });
}
