// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview E2E test suite for chrome://personalization.
 */

GEN('#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_browsertest_fixture.h"');

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "ash/public/cpp/ambient/ambient_client.h"');
GEN('#include "content/public/test/browser_test.h"');

const ROOT_PAGE = 'chrome://personalization/';
const DEFAULT_WALLPAPER_NAME = 'Default Wallpaper';

/**
 * Wait until `func` returns a truthy value.
 * If `timeoutMs` milliseconds elapse, will reject with `message`.
 * `message` may either be a string, or function. It will be called with the
 * final value returned by `func`. Polls every `intervalMs` milliseconds.
 * Resolves with the final value of `func`.
 */
async function waitUntil(func, message, intervalMs = 50, timeoutMs = 1001) {
  const messageType = typeof message;
  if (messageType !== 'string' && messageType !== 'function') {
    throw new Error(
        `message must be a string|function but received ${messageType}`);
  }
  let rejectTimer = null;
  let pollTimer = null;
  let value;

  function cleanup() {
    if (rejectTimer) {
      window.clearTimeout(rejectTimer);
    }
    if (pollTimer) {
      window.clearInterval(pollTimer);
    }
  }

  return new Promise((resolve, reject) => {
    rejectTimer = window.setTimeout(() => {
      cleanup();
      const errorMessage =
          messageType === 'function' ? message(value) : message;
      reject(new Error(errorMessage));
    }, timeoutMs);

    pollTimer = window.setInterval(() => {
      value = func();
      if (value) {
        cleanup();
        resolve(value);
      }
    }, intervalMs);
  });
}

function getRouter() {
  return document.querySelector('personalization-router');
}

class PersonalizationAppBrowserTest extends testing.Test {
  /** @override */
  get browsePreload() {
    return ROOT_PAGE;
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kWallpaperGooglePhotosSharedAlbums',
      ],
    };
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get typedefCppFixture() {
    return 'ash::personalization_app::PersonalizationAppBrowserTestFixture';
  }
}



// TODO(crbug/1262025) revisit this workaround for js2gtest requiring "var"
// declarations.
this[PersonalizationAppBrowserTest.name] = PersonalizationAppBrowserTest;

// Tests that chrome://personalization loads the page and various contents
// without javascript errors or a 404 or crash. Displays user preview, wallpaper
// preview, and ambient preview.
TEST_F(PersonalizationAppBrowserTest.name, 'All', async () => {
  await import('chrome://webui-test/mojo_webui_test_support.js');

  suite('main page', () => {
    test('has root page content', () => {
      assertEquals(document.location.href, ROOT_PAGE);
      const userPreview = getRouter()
                              .shadowRoot.querySelector('personalization-main')
                              .shadowRoot.querySelector('user-preview');
      const wallpaperPreview =
          getRouter()
              .shadowRoot.querySelector('personalization-main')
              .shadowRoot.querySelector('wallpaper-preview');
      assertTrue(!!userPreview);
      assertTrue(!!wallpaperPreview);
    });

    test('shows theme buttons', async () => {
      const theme = getRouter()
                        .shadowRoot.querySelector('personalization-main')
                        .shadowRoot.querySelector('personalization-theme');
      const lightButton = await waitUntil(
          () => theme.shadowRoot.getElementById('lightMode'),
          'failed to find light button');
      assertEquals('true', lightButton.getAttribute('aria-pressed'));
      const darkButton = theme.shadowRoot.getElementById('darkMode');
      assertTrue(!!darkButton);
      assertEquals(darkButton.getAttribute('aria-pressed'), 'false');
    });

    test('shows user info', async () => {
      const preview = getRouter()
                          .shadowRoot.querySelector('personalization-main')
                          .shadowRoot.querySelector('user-preview');

      const email = await waitUntil(
          () => preview.shadowRoot.getElementById('email'),
          'failed to find user email');
      assertEquals('fake-email', email.innerText);
      assertEquals(
          'Fake Name', preview.shadowRoot.getElementById('name').innerText);
    });
  });

  mocha.run();
});

class PersonalizationAppAmbientModeAllowedBrowserTest extends
    PersonalizationAppBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      GEN('ash::AmbientClient::Get()->SetAmbientModeAllowedForTesting(true);');
    };
  }
}

this[PersonalizationAppAmbientModeAllowedBrowserTest.name] =
    PersonalizationAppAmbientModeAllowedBrowserTest;

TEST_F(
    PersonalizationAppAmbientModeAllowedBrowserTest.name, 'All', async () => {
      await import('chrome://webui-test/mojo_webui_test_support.js');

      suite('ambient mode allowed', () => {
        test('shows ambient preview', () => {
          const preview =
              getRouter()
                  .shadowRoot.querySelector('personalization-main')
                  .shadowRoot.querySelector('ambient-preview-large');
          assertTrue(!!preview);
        });

        test('shows ambient subpage link', () => {
          const ambientSubpageLink =
              getRouter()
                  .shadowRoot.querySelector('personalization-main')
                  .shadowRoot.querySelector('ambient-preview-large')
                  .shadowRoot.querySelector('#ambientSubpageLink');
          assertTrue(!!ambientSubpageLink);
        });
      });
      mocha.run();
    });

class PersonalizationAppAmbientModeDisallowedBrowserTest extends
    PersonalizationAppBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      GEN('ash::AmbientClient::Get()->SetAmbientModeAllowedForTesting(false);');
    };
  }
}

this[PersonalizationAppAmbientModeDisallowedBrowserTest.name] =
    PersonalizationAppAmbientModeDisallowedBrowserTest;

TEST_F(
    PersonalizationAppAmbientModeDisallowedBrowserTest.name, 'All',
    async () => {
      await import('chrome://webui-test/mojo_webui_test_support.js');

      suite('ambient mode disallowed', () => {
        test('does not show ambient preview', () => {
          const preview = getRouter()
                              .shadowRoot.querySelector('personalization-main')
                              .shadowRoot.querySelector('ambient-preview');
          assertFalse(!!preview);
        });

        test('does not show ambient subpage link', () => {
          const ambientSubpageLink =
              getRouter()
                  .shadowRoot.querySelector('personalization-main')
                  .shadowRoot.getElementById('ambientSubpageLink');
          assertFalse(!!ambientSubpageLink);
        });
      });

      mocha.run();
    });

class PersonalizationAppWallpaperSubpageBrowserTest extends
    PersonalizationAppBrowserTest {}

this[PersonalizationAppWallpaperSubpageBrowserTest.name] =
    PersonalizationAppWallpaperSubpageBrowserTest;

TEST_F(PersonalizationAppWallpaperSubpageBrowserTest.name, 'All', async () => {
  await import('chrome://webui-test/mojo_webui_test_support.js');

  function clickWallpaperPreviewLink() {
    assertEquals(
        ROOT_PAGE, window.location.href,
        'wallpaper preview link only present on root page');
    getRouter()
        .shadowRoot.querySelector('personalization-main')
        .shadowRoot.querySelector('wallpaper-preview')
        .shadowRoot.getElementById('wallpaperButton')
        .click();
    assertEquals(
        ROOT_PAGE + 'wallpaper', window.location.href,
        'should have navigated to wallpaper');
  }

  function getWallpaperSubpage() {
    const router = getRouter();
    assertTrue(!!router, 'personalization-router should be top level element');

    const wallpaperSubpage =
        router.shadowRoot.querySelector('wallpaper-subpage');
    assertTrue(
        !!wallpaperSubpage,
        'wallpaper-subpage should be found under personalization-router');

    return wallpaperSubpage;
  }

  function getWallpaperSelected() {
    const subpage = getWallpaperSubpage();
    const wallpaperSelected =
        subpage.shadowRoot.querySelector('wallpaper-selected');
    assertTrue(!!wallpaperSelected, 'wallpaper-selected should exist');
    return wallpaperSelected;
  }

  setup(async () => {
    // Reset to default state before each test to reduce order dependencies.
    await window.personalizationTestApi.reset();
    clickWallpaperPreviewLink();
  });

  suite('wallpaper subpage', () => {
    // Tests that chrome://personalization/wallpaper runs js file and that it
    // goes somewhere instead of 404ing or crashing.
    test('has wallpaper subpage url', () => {
      const title = document.querySelector('head > title');
      assertEquals('Wallpaper', title.innerText);
    });

    test('loads collections grid', () => {
      const wallpaperSubpage = getWallpaperSubpage();

      const collections =
          wallpaperSubpage.shadowRoot.querySelector('wallpaper-collections');
      assertTrue(
          !!collections,
          'wallpaper-collections should be found under wallpaper-subpage');

      assertFalse(
          collections.parentElement.hidden, 'parent element should be visible');
      assertGT(
          collections.offsetWidth, 0,
          'wallpaper-collections should have visible width');
      assertGT(
          collections.offsetHeight, 0,
          'wallpaper-collections grid should have visible height');
    });
  });

  suite('backdrop', function() {
    test('selects wallpaper', async () => {
      const wallpaperSelected = getWallpaperSelected();
      const textContainer =
          wallpaperSelected.shadowRoot.getElementById('textContainer');
      assertEquals(
          DEFAULT_WALLPAPER_NAME,
          textContainer.querySelector('#imageTitle').textContent.trim(),
          'default wallpaper is shown at first');

      const subpage = getWallpaperSubpage();
      const collections =
          subpage.shadowRoot.querySelector('wallpaper-collections');

      const onlineTileToClick = await waitUntil(
          () =>
              Array
                  .from(collections.shadowRoot.querySelectorAll(
                      `wallpaper-grid-item[aria-disabled='false'][data-online]`))
                  .find(tile => tile.primaryText === 'Test Collection 2'),
          'waiting for online tile with title Test Collection 2 to load');

      onlineTileToClick.click();

      const wallpaperImages = await waitUntil(
          () => subpage.shadowRoot.querySelector('wallpaper-images'),
          'failed selecting wallpaper-images');

      assertFalse(wallpaperImages.hidden, 'wallpaper images now visible');
      assertGT(
          wallpaperImages.offsetWidth, 0,
          'wallpaper-images should have visible width');
      assertGT(
          wallpaperImages.offsetHeight, 0,
          'wallpaper-images should have visible height');

      const gridItem = await waitUntil(
          () => wallpaperImages.shadowRoot.querySelector(
              'wallpaper-grid-item:not([placeholder]):nth-of-type(2)'),
          'failed waiting for grid items to load');

      assertFalse(gridItem.selected, 'wallpaper tile does not start selected');
      gridItem.click();

      const expectedImageTitle =
          'fake_attribution_fake_collection_id_2_asset_id_31_line_0';
      await waitUntil(
          () =>
              textContainer.querySelector('#imageTitle').textContent.trim() ===
              expectedImageTitle,
          () => `failed waiting for expected image title ` +
              `${expectedImageTitle} after selecting wallpaper. ` +
              `html:\n${textContainer.outerHTML}`);

      assertEquals(
          'fake_attribution_fake_collection_id_2_asset_id_31_line_1',
          textContainer.querySelector('span:last-of-type').textContent.trim());

      assertTrue(gridItem.selected, 'wallpaper tile is selected after click');
    });
  });

  suite('google photos', () => {
    async function openGooglePhotos() {
      const subpage = getWallpaperSubpage();

      const googlePhotosTile = await waitUntil(
          () => subpage.shadowRoot.querySelector('wallpaper-collections')
                    .shadowRoot.querySelector(
                        `wallpaper-grid-item[aria-disabled='false']` +
                        `[data-google-photos]`),
          'failed waiting for google photos tile to load');
      googlePhotosTile.click();

      return await waitUntil(
          () => subpage.shadowRoot.querySelector('google-photos-collection'),
          'failed to find google photos collection');
    }

    async function openGooglePhotosAlbums() {
      const googlePhotosCollection = await openGooglePhotos();
      const albumsTabButton = await waitUntil(
          () => googlePhotosCollection.shadowRoot.getElementById('albumsTab'),
          'failed to find google photos album tab');

      albumsTabButton.click();

      return await waitUntil(
          () => googlePhotosCollection.shadowRoot.querySelector(
              'google-photos-albums'),
          'failed to find albums');
    }

    async function openGooglePhotosSharedAlbumById(
        albumId, googlePhotosAlbumsPromise = openGooglePhotosAlbums()) {
      const googlePhotosAlbums = await googlePhotosAlbumsPromise;

      const ariaLabel = `${albumId} Shared`;

      const sharedAlbumToClick = await waitUntil(
          () => googlePhotosAlbums.shadowRoot.querySelector(
              `wallpaper-grid-item[aria-disabled='false'][aria-label='${
                  ariaLabel}']`),
          'failed to get shared album');
      sharedAlbumToClick.click();
    }

    // Returns null when the dialog is not open.
    function getSharedAlbumDialog() {
      const wallpaperSelected = getWallpaperSelected();
      return wallpaperSelected.shadowRoot.querySelector(
          'google-photos-shared-album-dialog');
    }

    test('shared album sets correct query parameters', async () => {
      const googlePhotosAlbums = await openGooglePhotosAlbums();

      assertEquals(
          '', location.search,
          'location.search should be empty before selecting shared album');

      const sharedAlbumId = 'fake_google_photos_shared_album_id_1';

      await openGooglePhotosSharedAlbumById(sharedAlbumId, googlePhotosAlbums);

      assertDeepEquals(
          new Map([
            ['googlePhotosAlbumId', sharedAlbumId],
            ['googlePhotosAlbumIsShared', 'true']
          ]),
          new Map(new URLSearchParams(location.search).entries()),
          'album id and is shared param should appear in location.search');
    });

    test('select shared album as daily refresh', async () => {
      const sharedAlbumId = 'fake_google_photos_shared_album_id_2';
      await openGooglePhotosSharedAlbumById(sharedAlbumId);

      const wallpaperSelected = getWallpaperSelected();
      const imageTitle =
          wallpaperSelected.shadowRoot.getElementById('imageTitle');
      assertEquals(
          DEFAULT_WALLPAPER_NAME, imageTitle.textContent.trim(),
          'default wallpaper is shown at first');

      wallpaperSelected.shadowRoot.getElementById('dailyRefresh').click();

      const sharedAlbumDialog = await waitUntil(
          () => getSharedAlbumDialog(),
          'failed to select google photos shared album dialog');

      const sharedAlbumDialogAcceptButton =
          sharedAlbumDialog.shadowRoot.getElementById('accept');
      sharedAlbumDialogAcceptButton.click();

      const dailyRefreshRegex =
          /^Daily Refresh\: fake_google_photos_photo_id_\d$/;
      await waitUntil(
          () => dailyRefreshRegex.test(imageTitle.textContent.trim()),
          () => `Expected Daily refresh text to match regex ` +
              `${dailyRefreshRegex.source} but received:\n` +
              `${imageTitle.outerHTML}`);

      assertEquals(
          null, getSharedAlbumDialog(),
          'google photos shared album dialog is gone');
    });
  });

  mocha.run();
});
