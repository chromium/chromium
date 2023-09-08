// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {kDefaultImageSymbol, LocalImagesElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('LocalImagesElementTest', function() {
  let localImagesElement: LocalImagesElement|null;

  let wallpaperProvider: TestWallpaperProvider;

  let personalizationStore: TestPersonalizationStore;

  /**
   * Get all currently visible photo loading placeholders.
   */
  function getLoadingPlaceholders(): WallpaperGridItemElement[] {
    if (!localImagesElement) {
      return [];
    }

    return Array.from(
        localImagesElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[placeholder]:not([hidden])`));
  }

  function getDefaultImageHtmlElement(): WallpaperGridItemElement|null {
    return localImagesElement!.shadowRoot!
        .querySelector<WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}[data-id="${
                kDefaultImageSymbol.toString()}"]`);
  }

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(localImagesElement);
    localImagesElement = null;
    await flushTasks();
  });

  test('displays a tile with no src for unloaded local images', async () => {
    personalizationStore.data.wallpaper.local = {
      images: wallpaperProvider.localImages,
      data: wallpaperProvider.localImageData,
    };
    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {[kDefaultImageSymbol]: false},
    };

    localImagesElement = initElement(LocalImagesElement);
    await waitAfterNextRender(localImagesElement);

    // Iron-list creates some extra dom elements as a scroll buffer and
    // hides them.  Only select visible elements here to get the real ones.
    let loadingPlaceholders = getLoadingPlaceholders();

    // Counts as loading if store.loading.local.data does not contain an
    // entry for the image. Therefore should be 2 loading tiles.
    assertEquals(2, loadingPlaceholders.length, 'first time 2 placeholders');

    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {
        'LocalImage0.png': true,
        'LocalImage1.png': true,
        [kDefaultImageSymbol]: false,
      },
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(localImagesElement);

    loadingPlaceholders = getLoadingPlaceholders();
    assertEquals(2, loadingPlaceholders.length, 'still 2 placeholders');

    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {
        'LocalImage0.png': false,
        'LocalImage1.png': true,
        [kDefaultImageSymbol]: false,
      },
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(localImagesElement);

    loadingPlaceholders = getLoadingPlaceholders();
    assertEquals(1, loadingPlaceholders.length, 'Only one loading placeholder');
  });

  test(
      'displays images for current local images that have successfully loaded',
      async () => {
        personalizationStore.data.wallpaper.local = {
          images: wallpaperProvider.localImages,
          data: wallpaperProvider.localImageData,
        };
        personalizationStore.data.wallpaper.loading.local = {
          images: false,
          data: {[kDefaultImageSymbol]: false},
        };

        localImagesElement = initElement(LocalImagesElement);

        const ironList =
            localImagesElement.shadowRoot!.querySelector('iron-list');
        assertTrue(!!ironList);

        // Both items are sent.
        assertEquals(
            2, ironList.items!.length, 'both images are sent to iron-list');

        // Set loading finished for first thumbnail.
        personalizationStore.data.wallpaper.loading.local.data = {
          'LocalImage0.png': false,
          [kDefaultImageSymbol]: false,
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(localImagesElement);

        assertEquals(2, ironList.items!.length);
        let gridItems = localImagesElement.shadowRoot!.querySelectorAll<
            WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}:not([placeholder]):not([hidden])`);
        assertEquals(1, gridItems.length);
        assertDeepEquals(
            {url: 'data:image/png;base64,localimage0data'}, gridItems![0]!.src);

        // Set loading failed for second thumbnail.
        personalizationStore.data.wallpaper.loading.local.data = {
          'LocalImage0.png': false,
          'LocalImage1.png': false,
          [kDefaultImageSymbol]: false,
        };
        personalizationStore.data.wallpaper.local.data = {
          'LocalImage0.png': {url: 'data:image/png;base64,localimage0data'},
          'LocalImage1.png': {url: ''},
          [kDefaultImageSymbol]: {url: ''},
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(localImagesElement);

        // Still only first thumbnail displayed.
        gridItems = localImagesElement.shadowRoot!.querySelectorAll<
            WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}:not([placeholder]):not([hidden])`);
        assertEquals(
            1, gridItems.length, 'still only first thumbnail displayed');
        assertDeepEquals(
            {url: 'data:image/png;base64,localimage0data'}, gridItems![0]!.src);
      });

  test('sets selected if image name matches currently selected', async () => {
    personalizationStore.data.wallpaper.local = {
      images: [
        {path: '/test/LocalImage0.png'},
        {path: '/test/LocalImage1.png'},
      ],
      data: {
        '/test/LocalImage0.png': {url: 'data:image/png;base64,localimage0data'},
        '/test/LocalImage1.png': {url: 'data:image/png;base64,localimage1data'},
        [kDefaultImageSymbol]: {url: ''},
      },
    };
    // Done loading.
    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {
        '/test/LocalImage0.png': false,
        '/test/LocalImage1.png': false,
        [kDefaultImageSymbol]: false,
      },
    };

    localImagesElement = initElement(LocalImagesElement);
    await waitAfterNextRender(localImagesElement);

    // iron-list pre-creates some extra DOM elements but marks them as
    // hidden. Ignore them here to only get visible images.
    const images = localImagesElement.shadowRoot!
                       .querySelectorAll<WallpaperGridItemElement>(
                           `${WallpaperGridItemElement.is}:not([hidden])`);

    assertEquals(2, images.length);
    // Every image is not selected.
    assertTrue(Array.from(images).every(image => !image.selected));

    personalizationStore.data.wallpaper.currentSelected = {
      ...wallpaperProvider.currentWallpaper,
      key: '/test/LocalImage1.png',
    };
    personalizationStore.notifyObservers();

    assertEquals(2, images.length);
    assertFalse(images[0]!.selected!);
    assertTrue(images[1]!.selected!);
  });

  test('images have proper aria label when loaded', async () => {
    personalizationStore.data.wallpaper.local = {
      images: wallpaperProvider.localImages,
      data: wallpaperProvider.localImageData,
    };
    // Done loading.
    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {
        'LocalImage0.png': false,
        'LocalImage1.png': false,
        [kDefaultImageSymbol]: false,
      },
    };

    localImagesElement = initElement(LocalImagesElement);
    await waitAfterNextRender(localImagesElement);

    // iron-list pre-creates some extra DOM elements but marks them as
    // hidden. Ignore them here to only get visible images.
    const images = localImagesElement.shadowRoot!
                       .querySelectorAll<WallpaperGridItemElement>(
                           `${WallpaperGridItemElement.is}:not([hidden])`);

    assertEquals(2, images.length);
    // Every image has aria-label set.
    assertEquals(
        images[0]!.getAttribute('aria-label'),
        wallpaperProvider.localImages![0]!.path, 'image 0 has aria label');
    assertEquals(
        images[1]!.getAttribute('aria-label'),
        wallpaperProvider.localImages![1]!.path, 'image 1 has aria label');
  });

  test('default image has proper aria label', async () => {
    personalizationStore.data.wallpaper.local = {
      images: [kDefaultImageSymbol],
      data: {[kDefaultImageSymbol]: wallpaperProvider.defaultImageThumbnail},
    };

    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {[kDefaultImageSymbol]: false},
    };

    localImagesElement = initElement(LocalImagesElement);
    await waitAfterNextRender(localImagesElement);

    const images = localImagesElement.shadowRoot!
                       .querySelectorAll<WallpaperGridItemElement>(
                           `${WallpaperGridItemElement.is}:not([hidden])`);

    assertEquals(1, images.length, 'only default image is present');
    assertEquals(
        images[0]!.getAttribute('aria-label'), 'Default Wallpaper',
        'default image has correct aria label');
  });

  test('click default image thumbnail resets wallpaper', async () => {
    personalizationStore.data.wallpaper.local = {
      images: [kDefaultImageSymbol, ...wallpaperProvider.localImages!],
      data: {
        ...wallpaperProvider.localImageData,
        [kDefaultImageSymbol]: wallpaperProvider.defaultImageThumbnail,
      },
    };

    localImagesElement = initElement(LocalImagesElement);
    await waitAfterNextRender(localImagesElement);

    const container = getDefaultImageHtmlElement();
    container!.click();

    await wallpaperProvider.whenCalled('selectDefaultImage');
  });

  test('default image thumbnail hidden when fails to load', async () => {
    personalizationStore.data.wallpaper.local = {
      images: [kDefaultImageSymbol],
      data: {[kDefaultImageSymbol]: {url: ''}},
    };

    localImagesElement = initElement(LocalImagesElement);

    await waitAfterNextRender(localImagesElement);

    assertEquals(
        null, getDefaultImageHtmlElement(),
        'default image container does not exist');

    personalizationStore.data.wallpaper.local = {
      images: [kDefaultImageSymbol],
      data: {[kDefaultImageSymbol]: wallpaperProvider.defaultImageThumbnail},
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(localImagesElement);

    assertTrue(
        !!getDefaultImageHtmlElement(), 'default image container does exist');
  });
});
