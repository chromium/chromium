// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCountText} from 'chrome://personalization/common/utils.js';
import {GooglePhotosAlbum} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {GooglePhotosAlbums} from 'chrome://personalization/trusted/wallpaper/google_photos_albums_element.js';
import {initializeGooglePhotosData} from 'chrome://personalization/trusted/wallpaper/wallpaper_controller.js';
import {WallpaperGridItem} from 'chrome://personalization/trusted/wallpaper/wallpaper_grid_item_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertNotReached} from 'chrome://webui-test/chai_assert.js';
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

  /** Scrolls the specified |element| until |predicate| returns true. */
  async function scrollElementUntil(
      element: HTMLElement, predicate: () => boolean) {
    const timeout = +new Date() + 1000;
    while (!predicate()) {
      element.scrollBy(0, 500);
      await waitAfterNextRender(googlePhotosAlbumsElement!);
      if (+new Date() > timeout) {
        assertNotReached('Timed out while scrolling.');
      }
    }
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
    const albums: GooglePhotosAlbum[] = [
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        title: 'Album 0',
        photoCount: 0,
        preview: {url: 'foo.com'}
      },
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        title: 'Album 1',
        photoCount: 1,
        preview: {url: 'bar.com'}
      },
      {
        id: '0a268a37-877a-4936-81d4-38cc84b0f596',
        title: 'Album 2',
        photoCount: 2,
        preview: {url: 'baz.com'}
      }
    ];

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums(albums);
    wallpaperProvider.setGooglePhotosCount(
        albums.reduce((photosCount, album) => {
          photosCount += album.photoCount;
          return photosCount;
        }, 0));

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbums, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The |personalizationStore| should be empty, so no albums should be
    // rendered initially.
    const albumSelector = 'wallpaper-grid-item:not([hidden]).album';
    assertEquals(querySelectorAll(albumSelector)!.length, 0);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The wallpaper controller is expected to impose max resolution.
    albums.forEach(album => album.preview.url += '=s512');

    // Verify that the expected |albums| are rendered.
    const albumEls = querySelectorAll(albumSelector) as WallpaperGridItem[];
    assertEquals(albumEls.length, albums.length);
    albumEls.forEach((albumEl, i) => {
      assertEquals(albumEl.imageSrc, albums[i]!.preview.url);
      assertEquals(albumEl.primaryText, albums[i]!.title);
      assertEquals(albumEl.secondaryText, getCountText(albums[i]!.photoCount));
    });
  });

  // TODO(dmblack): Re-enable after branch.
  test.skip('incrementally loads albums', async () => {
    // Set photos count returned by |wallpaperProvider|.
    const photosCount = 200;
    wallpaperProvider.setGooglePhotosCount(photosCount);

    // Set initial list of albums returned by |wallpaperProvider|.
    const albumsCount = 200;
    let nextAlbumId = 1;
    wallpaperProvider.setGooglePhotosAlbums(
        Array.from({length: albumsCount / 2}).map(() => {
          return {
            id: `id-${nextAlbumId}`,
            title: `title-${nextAlbumId}`,
            photoCount: photosCount / albumsCount,
            preview: {url: `url-${nextAlbumId++}`}
          };
        }));

    // Set initial albums resume token returned by |wallpaperProvider|. When
    // resume token is defined, it indicates additional albums exist.
    const resumeToken = 'resumeToken';
    wallpaperProvider.setGooglePhotosAlbumsResumeToken(resumeToken);

    // Initialize Google Photos data in |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    assertEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums'),
        /*resumeToken=*/ null);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosAlbums');

    // Set the next list of albums returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums(
        Array.from({length: albumsCount / 2}).map(() => {
          return {
            id: `id-${nextAlbumId}`,
            title: `title-${nextAlbumId}`,
            photoCount: photosCount / albumsCount,
            preview: {url: `url-${nextAlbumId++}`}
          };
        }));

    // Set the next albums resume token returned by |wallpaperProvider|. When
    // resume token is undefined, it indicates no additional albums exist.
    wallpaperProvider.setGooglePhotosAlbumsResumeToken(undefined);

    // Restrict the viewport so that |googlePhotosAlbumsElement| will lazily
    // create albums instead of creating them all at once.
    const style = document.createElement('style');
    style.appendChild(document.createTextNode(`
      html,
      body {
        height: 100%;
        width: 100%;
      }
    `));
    document.head.appendChild(style);

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbums, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Register an event listener to cache whether the |gridScrollThreshold| has
    // been reached.
    let gridScrollThresholdReached = false;
    const gridScrollThreshold = googlePhotosAlbumsElement.$.gridScrollThreshold;
    gridScrollThreshold.addEventListener('lower-threshold', () => {
      gridScrollThresholdReached = true;
    });

    // Scroll until the |gridScrollThreshold| is reached.
    await scrollElementUntil(gridScrollThreshold, () => {
      return gridScrollThresholdReached;
    });

    // Wait for and verify that the next batch of albums has been
    // requested.
    assertEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums'),
        resumeToken);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosAlbums');

    // Scroll until the bottom of the grid is reached.
    let scrollTop = -1;
    await scrollElementUntil(gridScrollThreshold, () => {
      const oldScrollTop = scrollTop;
      scrollTop = gridScrollThreshold.scrollTop;
      return scrollTop === oldScrollTop;
    });

    // Verify that no next batch of albums has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosAlbums'), 0);
  });
}
