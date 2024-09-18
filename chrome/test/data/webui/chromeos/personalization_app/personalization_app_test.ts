// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DynamicColorElement, getThemeProvider, GooglePhotosAlbumsElement, GooglePhotosCollectionElement, GooglePhotosSharedAlbumDialogElement, PersonalizationRouterElement, PersonalizationThemeElement, SeaPenFeedbackElement, SeaPenFreeformElement, SeaPenImagesElement, SeaPenInputQueryElement, SeaPenPaths, SeaPenRecentWallpapersElement, SeaPenRouterElement, SeaPenSamplesElement, SeaPenTemplateQueryElement, setTransitionsEnabled, WallpaperCollectionsElement, WallpaperGridItemElement, WallpaperImagesElement} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertLE, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * @fileoverview E2E test suite for chrome://personalization.
 */

const ROOT_PAGE = 'chrome://personalization/';
const DEFAULT_WALLPAPER_NAME = 'Default Wallpaper';

/**
 * Wait until `func` returns a truthy value.
 * If `timeoutMs` milliseconds elapse, will reject with `message`.
 * `message` may either be a string, or function. It will be called with the
 * final value returned by `func`. Polls every `intervalMs` milliseconds.
 * Resolves with the final value of `func`.
 */
async function waitUntil<T>(
    func: () => T, message: string|((value: T) => string),
    intervalMs: number = 50,
    timeoutMs: number = 1001): Promise<NonNullable<T>> {
  const messageType = typeof message;
  if (messageType !== 'string' && messageType !== 'function') {
    throw new Error(
        `message must be a string|function but received ${messageType}`);
  }
  let rejectTimer: number|null = null;
  let pollTimer: number|null = null;
  let value: T;

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
          typeof message === 'function' ? message(value) : message;
      reject(new Error(errorMessage));
    }, timeoutMs);

    pollTimer = window.setInterval(() => {
      value = func();
      if (value) {
        cleanup();
        resolve(value!);
      }
    }, intervalMs);
  });
}

function getRouter(): PersonalizationRouterElement {
  return PersonalizationRouterElement.instance();
}

/** Returns an array of three numbers, representing the RGB values. */
function getBodyColorChannels() {
  return getComputedStyle(document.body)
      .backgroundColor.match(/rgb\((\d+), (\d+), (\d+)\)/)!.slice(1, 4)
      .map(x => parseInt(x, 10));
}

suite('main page', () => {
  setup(() => {
    // Disables transition animation for tests.
    setTransitionsEnabled(false);
  });

  // Tests that chrome://personalization loads the page and various contents
  // without javascript errors or a 404 or crash. Displays user preview,
  // wallpaper preview, ambient preview, and dynamic color controls.

  test('has root page content', () => {
    assertEquals(document.location.href, ROOT_PAGE);
    const userPreview = getRouter()
                            .shadowRoot?.querySelector('personalization-main')
                            ?.shadowRoot?.querySelector('user-preview');
    const wallpaperPreview =
        getRouter()
            .shadowRoot?.querySelector('personalization-main')
            ?.shadowRoot?.querySelector('wallpaper-preview');
    assertTrue(!!userPreview);
    assertTrue(!!wallpaperPreview);
  });

  test('shows theme buttons', async () => {
    const theme = getRouter()
                      .shadowRoot?.querySelector('personalization-main')
                      ?.shadowRoot?.querySelector('personalization-theme');

    const lightButton = await waitUntil(
        () => theme?.shadowRoot?.getElementById('lightMode'),
        'failed to find light button');
    assertEquals('false', lightButton.getAttribute('aria-checked'));
    const darkButton = theme?.shadowRoot?.getElementById('darkMode');
    assertTrue(!!darkButton);
    assertEquals('false', darkButton.getAttribute('aria-checked'));
    const autoButton = theme?.shadowRoot?.getElementById('autoMode');
    assertTrue(!!autoButton);
    assertEquals('true', autoButton.getAttribute('aria-checked'));
  });

  test('selects dark mode', async () => {
    const theme = getRouter()
                      .shadowRoot?.querySelector('personalization-main')
                      ?.shadowRoot?.querySelector('personalization-theme');
    const darkButton = theme?.shadowRoot?.getElementById('darkMode');
    assertTrue(!!darkButton, '#darkMode exists');

    darkButton.click();

    assertEquals('true', darkButton.getAttribute('aria-checked'));
    await waitUntil(
        () => getBodyColorChannels().every(channel => channel < 50),
        'failed to switch to dark mode');
  });

  test('selects light mode', async () => {
    const theme = getRouter()
                      .shadowRoot?.querySelector('personalization-main')
                      ?.shadowRoot?.querySelector('personalization-theme');
    const lightButton = theme?.shadowRoot?.getElementById('lightMode');
    assertTrue(!!lightButton, '#lightMode exists');

    lightButton.click();

    assertEquals('true', lightButton.getAttribute('aria-checked'));
    await waitUntil(
        () => getBodyColorChannels().every(channel => channel > 200),
        'failed to switch to light mode');
  });

  test('shows user info', async () => {
    const preview = getRouter()
                        .shadowRoot?.querySelector('personalization-main')
                        ?.shadowRoot?.querySelector('user-preview');

    const email = await waitUntil(
        () => preview?.shadowRoot?.getElementById('email'),
        'failed to find user email');
    assertEquals('fake-email', email.innerText);
    assertEquals(
        'Fake Name', preview?.shadowRoot?.getElementById('name')?.innerText);
  });
});


suite('ambient mode allowed', () => {
  setup(() => {
    // Disables transition animation for tests.
    setTransitionsEnabled(false);
  });

  test('shows ambient preview', () => {
    const preview = getRouter()
                        .shadowRoot?.querySelector('personalization-main')
                        ?.shadowRoot?.querySelector('ambient-preview-large');
    assertTrue(!!preview);
  });

  test('shows ambient subpage link', () => {
    const ambientSubpageLink =
        getRouter()
            .shadowRoot?.querySelector('personalization-main')
            ?.shadowRoot?.querySelector('ambient-preview-large')
            ?.shadowRoot?.querySelector('cr-icon-button');
    assertTrue(!!ambientSubpageLink);
  });
});


suite('ambient mode disallowed', () => {
  setup(() => {
    // Disables transition animation for tests.
    setTransitionsEnabled(false);
  });

  test('shows ambient preview', () => {
    const preview = getRouter()
                        .shadowRoot?.querySelector('personalization-main')
                        ?.shadowRoot?.querySelector('ambient-preview-large');

    assertTrue(!!preview, 'ambient-preview-large exists');
  });

  test('shows disabled ambient subpage link', () => {
    const ambientSubpageLink =
        getRouter()
            .shadowRoot?.querySelector('personalization-main')
            ?.shadowRoot?.querySelector('ambient-preview-large')
            ?.shadowRoot?.querySelector('cr-icon-button');

    assertTrue(!!ambientSubpageLink, 'ambient subpage link exists');
    assertTrue(ambientSubpageLink.disabled, 'ambient subpage link is disabled');
  });

  test('shows help link', () => {
    const helpLink = getRouter()
                         .shadowRoot?.querySelector('personalization-main')
                         ?.shadowRoot?.querySelector('ambient-preview-large')
                         ?.shadowRoot?.querySelector('cr-button')
                         ?.firstElementChild?.getAttribute('href');
    assertTrue(!!helpLink, 'help link exists');
    assertTrue(
        helpLink.includes('support.google.com'), 'help link content matches');
  });
});

suite('wallpaper subpage', () => {
  function clickWallpaperPreviewLink() {
    assertEquals(
        ROOT_PAGE, window.location.href,
        'wallpaper preview link only present on root page');
    getRouter()
        .shadowRoot?.querySelector('personalization-main')
        ?.shadowRoot?.querySelector('wallpaper-preview')
        ?.shadowRoot?.querySelector('cr-icon-button')
        ?.click();
    assertEquals(
        ROOT_PAGE + 'wallpaper', window.location.href,
        'should have navigated to wallpaper');
  }

  function getWallpaperSubpage() {
    const router = getRouter();
    assertTrue(!!router, 'personalization-router should be top level element');

    const wallpaperSubpage =
        router.shadowRoot?.querySelector('wallpaper-subpage');
    assertTrue(
        !!wallpaperSubpage,
        'wallpaper-subpage should be found under personalization-router');

    return wallpaperSubpage;
  }

  function getWallpaperSelected() {
    const subpage = getWallpaperSubpage();
    const wallpaperSelected =
        subpage.shadowRoot?.querySelector('wallpaper-selected');
    assertTrue(!!wallpaperSelected, 'wallpaper-selected should exist');
    return wallpaperSelected;
  }

  setup(async () => {
    // Reset to default state before each test to reduce order dependencies.
    await window.personalizationTestApi.reset();

    // Disables transition animation for tests.
    setTransitionsEnabled(false);

    clickWallpaperPreviewLink();
  });

  // Tests that chrome://personalization/wallpaper runs js file and that it
  // goes somewhere instead of 404ing or crashing.
  test('has wallpaper subpage url', () => {
    const title = document.querySelector('head > title');
    assertEquals('Wallpaper', (title as HTMLElement).innerText);
  });

  test('loads collections grid', () => {
    const wallpaperSubpage = getWallpaperSubpage();

    const collections =
        wallpaperSubpage.shadowRoot?.querySelector<WallpaperCollectionsElement>(
            'wallpaper-collections');
    assertTrue(
        !!collections,
        'wallpaper-collections should be found under wallpaper-subpage');

    assertFalse(
        collections.parentElement!.hidden, 'parent element should be visible');
    assertGT(
        collections.offsetWidth, 0,
        'wallpaper-collections should have visible width');
    assertGT(
        collections.offsetHeight, 0,
        'wallpaper-collections grid should have visible height');
  });

  suite('backdrop', function() {
    setup(() => {
      // Disables transition animation for tests.
      setTransitionsEnabled(false);
    });

    test('selects wallpaper', async () => {
      const wallpaperSelected = getWallpaperSelected();
      const textContainer =
          wallpaperSelected.shadowRoot?.getElementById('textContainer');
      assertTrue(!!textContainer, 'wallpaper text container exists');
      assertEquals(
          DEFAULT_WALLPAPER_NAME,
          textContainer?.querySelector('#imageTitle')?.textContent?.trim(),
          'default wallpaper is shown at first');

      const subpage = getWallpaperSubpage();
      const collections =
          subpage.shadowRoot?.querySelector('wallpaper-collections');

      const onlineTileToClick = await waitUntil(
          () =>
              Array
                  .from(collections!.shadowRoot!.querySelectorAll<
                        WallpaperGridItemElement>(
                      `wallpaper-grid-item[aria-disabled='false'][data-online]`))
                  .find(tile => tile.primaryText === 'Test Collection 2'),
          'waiting for online tile with title Test Collection 2 to load');

      onlineTileToClick.click();

      const wallpaperImages = await waitUntil(
          () => subpage.shadowRoot?.querySelector<WallpaperImagesElement>(
              'wallpaper-images'),
          'failed selecting wallpaper-images');

      assertFalse(wallpaperImages.hidden, 'wallpaper images now visible');
      assertGT(
          wallpaperImages.offsetWidth, 0,
          'wallpaper-images should have visible width');
      assertGT(
          wallpaperImages.offsetHeight, 0,
          'wallpaper-images should have visible height');

      const gridItem = await waitUntil(
          () =>
              wallpaperImages.shadowRoot
                  ?.querySelector<WallpaperGridItemElement>(
                      'wallpaper-grid-item:not([placeholder]):nth-of-type(2)'),
          'failed waiting for grid items to load');

      assertFalse(gridItem.selected!, 'wallpaper tile does not start selected');
      gridItem.click();

      const expectedImageTitle =
          'fake_attribution_fake_collection_id_2_asset_id_41_line_0';

      await waitUntil(
          () => textContainer.querySelector('#imageTitle')
                    ?.textContent?.trim() === expectedImageTitle,
          () => `failed waiting for expected image title ` +
              `${expectedImageTitle} after selecting wallpaper. ` +
              `html:\n${textContainer.outerHTML}`,
          /*intervalMs=*/ 500,
          /*timeoutMs=*/ 3001);

      assertEquals(
          'fake_attribution_fake_collection_id_2_asset_id_41_line_1',
          textContainer.querySelector('span:last-of-type')
              ?.textContent?.trim());

      assertTrue(gridItem.selected!, 'wallpaper tile is selected after click');
    });
  });

  suite('google photos', () => {
    setup(() => {
      // Disables transition animation for tests.
      setTransitionsEnabled(false);
    });

    async function openGooglePhotos(): Promise<GooglePhotosCollectionElement> {
      const subpage = getWallpaperSubpage();

      const googlePhotosTile = await waitUntil(
          () => subpage.shadowRoot?.querySelector('wallpaper-collections')
                    ?.shadowRoot?.querySelector<WallpaperGridItemElement>(
                        `wallpaper-grid-item[aria-disabled='false']` +
                        `[data-google-photos]`),
          'failed waiting for google photos tile to load');
      googlePhotosTile.click();

      return await waitUntil(
          () => subpage.shadowRoot?.querySelector('google-photos-collection'),
          'failed to find google photos collection');
    }

    async function openGooglePhotosAlbums():
        Promise<GooglePhotosAlbumsElement> {
      const googlePhotosCollection = await openGooglePhotos();
      const albumsTabButton = await waitUntil(
          () => googlePhotosCollection.shadowRoot?.getElementById('albumsTab'),
          'failed to find google photos album tab');

      albumsTabButton.click();

      return await waitUntil(
          () => googlePhotosCollection.shadowRoot?.querySelector(
              'google-photos-albums'),
          'failed to find albums');
    }

    async function openGooglePhotosSharedAlbumById(
        albumId: string, googlePhotosAlbumsPromise = openGooglePhotosAlbums()) {
      const googlePhotosAlbums = await googlePhotosAlbumsPromise;

      const ariaLabel = `${albumId} Shared`;

      const sharedAlbumToClick = await waitUntil(
          () =>
              googlePhotosAlbums.shadowRoot
                  ?.querySelector<WallpaperGridItemElement>(
                      `wallpaper-grid-item[aria-disabled='false'][aria-label='${
                          ariaLabel}']`),
          'failed to get shared album');
      sharedAlbumToClick.click();
    }

    // Returns null when the dialog is not open.
    function getSharedAlbumDialog(): GooglePhotosSharedAlbumDialogElement|null {
      const wallpaperSelected = getWallpaperSelected();
      return wallpaperSelected.shadowRoot!.querySelector(
          'google-photos-shared-album-dialog');
    }

    test('shared album sets correct query parameters', async () => {
      const googlePhotosAlbums = await openGooglePhotosAlbums();

      assertEquals(
          '', location.search,
          'location.search should be empty before selecting shared album');

      const sharedAlbumId = 'fake_google_photos_shared_album_id_1';

      await openGooglePhotosSharedAlbumById(
          sharedAlbumId, Promise.resolve(googlePhotosAlbums));

      assertDeepEquals(
          new Map([
            ['googlePhotosAlbumId', sharedAlbumId],
            ['googlePhotosAlbumIsShared', 'true'],
          ]),
          new Map(new URLSearchParams(location.search).entries()),
          'album id and is shared param should appear in location.search');
    });

    test('select shared album as daily refresh', async () => {
      const sharedAlbumId = 'fake_google_photos_shared_album_id_2';
      await openGooglePhotosSharedAlbumById(sharedAlbumId);

      const wallpaperSelected = getWallpaperSelected();
      const imageTitle =
          wallpaperSelected.shadowRoot?.getElementById('imageTitle');
      assertTrue(!!imageTitle, 'image title exists');
      assertEquals(
          DEFAULT_WALLPAPER_NAME, imageTitle.textContent?.trim(),
          'default wallpaper is shown at first');

      const dailyRefreshButton = await waitUntil(
          () => wallpaperSelected.shadowRoot?.getElementById('dailyRefresh'),
          'failed to find daily refresh button');
      dailyRefreshButton.click();

      const sharedAlbumDialog = await waitUntil(
          () => getSharedAlbumDialog(),
          'failed to select google photos shared album dialog');

      const sharedAlbumDialogAcceptButton =
          sharedAlbumDialog.shadowRoot?.getElementById('accept');
      sharedAlbumDialogAcceptButton?.click();

      const dailyRefreshRegex =
          /^Daily Refresh\: fake_google_photos_photo_id_\d$/;
      await waitUntil(
          () => dailyRefreshRegex.test(imageTitle.textContent!.trim()),
          () => `Expected Daily refresh text to match regex ` +
              `${dailyRefreshRegex.source} but received:\n` +
              `${imageTitle.outerHTML}`,
          /*intervalMs=*/ 500,
          /*timeoutMs=*/ 3001);

      assertEquals(
          null, getSharedAlbumDialog(),
          'google photos shared album dialog is gone');
    });
  });
});


suite('sea pen', () => {
  function clickWallpaperPreviewLink() {
    assertEquals(
        ROOT_PAGE, window.location.href,
        'wallpaper preview link only present on root page');
    getRouter()
        .shadowRoot?.querySelector('personalization-main')
        ?.shadowRoot?.querySelector('wallpaper-preview')
        ?.shadowRoot?.querySelector('cr-icon-button')
        ?.click();
    assertEquals(
        ROOT_PAGE + 'wallpaper', window.location.href,
        'should have navigated to wallpaper');
  }

  function getWallpaperSubpage() {
    const router = getRouter();
    assertTrue(!!router, 'personalization-router should be top level element');

    const wallpaperSubpage =
        router.shadowRoot?.querySelector('wallpaper-subpage');
    assertTrue(
        !!wallpaperSubpage,
        'wallpaper-subpage should be found under personalization-router');

    return wallpaperSubpage;
  }

  async function getSeaPenRouter(): Promise<SeaPenRouterElement> {
    const subpage = getWallpaperSubpage();
    const seaPenTile = await waitUntil(
        () => subpage.shadowRoot?.querySelector('wallpaper-collections')
                  ?.shadowRoot?.querySelector<WallpaperGridItemElement>(
                      `wallpaper-grid-item[aria-disabled='false']` +
                      `[data-sea-pen]`),
        'waiting for sea-pen-tile',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    seaPenTile.click();
    const seaPenRouter = await waitUntil(
        () => getRouter().shadowRoot?.querySelector<SeaPenRouterElement>(
            'sea-pen-router')!,
        'waiting for sea-pen-router',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    return seaPenRouter;
  }

  async function getSeaPenTemplateQuery(templateIndex: number):
      Promise<SeaPenTemplateQueryElement> {
    const seaPenRouter = SeaPenRouterElement.instance();
    const templates = await waitUntil(
        () => seaPenRouter.shadowRoot?.querySelector('sea-pen-templates')
                  ?.shadowRoot?.querySelectorAll<WallpaperGridItemElement>(
                      `wallpaper-grid-item[data-sea-pen-image]`),
        'waiting for sea-pen-tile',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    templates[templateIndex]!.click();

    return await waitUntil(
        () =>
            seaPenRouter.shadowRoot?.querySelector<SeaPenTemplateQueryElement>(
                'sea-pen-template-query')!,
        'waiting for sea-pen-template-query',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
  }

  function getSeaPenTemplatePrompt(
      seaPenTemplateQuery: SeaPenTemplateQueryElement): string {
    const templateTokens =
        seaPenTemplateQuery.shadowRoot!.querySelectorAll<HTMLElement>(
            '.chip-container, .template-text');
    assertTrue(
        templateTokens.length > 0, 'template tokens should be available');

    const templateText: string[] = [];
    for (const token of templateTokens) {
      if (token.style.display === 'none') {
        continue;
      }
      if (token.classList.contains('template-text')) {
        templateText.push(token.innerText);
        continue;
      }
      const chipTextElement =
          (token as HTMLElement)
              .querySelector<HTMLElement>('sea-pen-chip-text');
      assertTrue(!!chipTextElement, 'sea-pen-chip-text should be available');
      const chipText =
          chipTextElement.shadowRoot?.getElementById('chipText')?.innerText;
      if (chipText) {
        templateText.push(chipText);
      }
    }
    return templateText.join(' ');
  }

  setup(async () => {
    // Disables transition animation for tests.
    setTransitionsEnabled(false);

    // Reset to default state before each test to reduce order dependencies.
    await window.personalizationTestApi.reset();

    clickWallpaperPreviewLink();
  });

  teardown(() => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: false});
  });

  suite('feedback', async () => {
    // At the end of this test, a feedback dialog is expected to be opened in an
    // external window.
    test(`open feedback dialog`, async () => {
      const seaPenRouter = await getSeaPenRouter();
      const seaPenTemplateQuery = await getSeaPenTemplateQuery(6);
      // Creates images.
      seaPenTemplateQuery.shadowRoot?.getElementById('searchButton')!.click();

      // Presses thumbs up feedback button.
      const seaPenImages = await waitUntil(
          () => seaPenRouter.shadowRoot?.querySelector<SeaPenImagesElement>(
              'sea-pen-images'),
          'waiting for sea-pen-images');

      const feedbacks = await waitUntil(
          () => Array.from(
              seaPenImages!.shadowRoot!.querySelectorAll<SeaPenFeedbackElement>(
                  `sea-pen-feedback`)),
          'waiting for thumbnails load');
      assertTrue(!!feedbacks, 'feedbacks should exist');

      const thumbsUpButton =
          feedbacks[0]?.shadowRoot?.getElementById('thumbsUp');
      assertTrue(!!thumbsUpButton, 'thumbsUpButton should exist');
      assertEquals(
          thumbsUpButton?.getAttribute('iron-icon'), 'cr:thumbs-up',
          'thumbsUpButton should not be filled');
      thumbsUpButton!.click();
      assertEquals(
          thumbsUpButton?.getAttribute('iron-icon'), 'cr:thumbs-up-filled',
          'thumbsUpButton should be filled');
    });
  });

  test('has selected wallpaper on root page', async () => {
    await getSeaPenRouter();

    const wallpaperSelected = await waitUntil(
        () => getRouter().shadowRoot?.getElementById('wallpaperSelected')!,
        'waiting for sea-pen-router wallpaper-selected', /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!wallpaperSelected, 'wallpaper-selected should exist');
  });

  test('hides selected wallpaper on non root page', async () => {
    await getSeaPenRouter();

    let wallpaperSelected =
        getRouter().shadowRoot?.getElementById('wallpaperSelected')!;
    assertTrue(!!wallpaperSelected, 'wallpaper-selected should exist');
    assertNotEquals(getComputedStyle(wallpaperSelected).display, 'none');

    const seaPenTemplateQuery = await getSeaPenTemplateQuery(0);
    assertTrue(!!seaPenTemplateQuery, 'sea-pen-template-query should exist');

    wallpaperSelected =
        getRouter().shadowRoot?.getElementById('wallpaperSelected')!;
    assertFalse(!!wallpaperSelected, 'wallpaper-selected should not exist');
  });

  test('show more option chips', async () => {
    const seaPenRouter = await getSeaPenRouter();

    const closeIntroductionButton = await waitUntil(
        () => seaPenRouter.shadowRoot
                  ?.querySelector('sea-pen-introduction-dialog')
                  ?.shadowRoot?.querySelector<HTMLElement>('#close'),
        'wait for close button to load');
    closeIntroductionButton.click();

    const seaPenTemplateQuery = await getSeaPenTemplateQuery(6);
    assertTrue(!!seaPenTemplateQuery, 'Characters template should show up');

    const seaPenChips = await waitUntil(
        () => seaPenTemplateQuery.shadowRoot?.querySelectorAll<HTMLDivElement>(
            '#template > .chip-container > .chip-text'),
        'waiting for chips');
    assertEquals(
        3, seaPenChips.length,
        'there should be 3 chips in the Characters template');
    seaPenChips[1]!.click();

    const seaPenOptions = await waitUntil(
        () => seaPenTemplateQuery.shadowRoot?.querySelector('sea-pen-options'),
        'waiting for sea-pen-options to load');

    let options = await waitUntil(
        () => seaPenOptions.shadowRoot?.querySelectorAll<CrButtonElement>(
            '.option'),
        'waiting for options to load');
    const numOptionsInitiallyShown = options.length;
    assertTrue(numOptionsInitiallyShown > 0, 'some options are shown');

    const expandButton = await waitUntil(
        () => seaPenOptions.shadowRoot?.querySelector<CrButtonElement>(
            '#expandButton'),
        'wait for expand button');
    assertTrue(!!expandButton, 'expand button should show up');

    expandButton.click();
    options = await waitUntil(
        () => seaPenOptions.shadowRoot?.querySelectorAll<CrButtonElement>(
            '.option'),
        'waiting for options to load');
    assertLE(
        numOptionsInitiallyShown, options.length,
        'more options should show up after clicking expand button');
  });

  [false, true].forEach((useInspire, i) => {
    test(`creates images with inspire ${useInspire}`, async () => {
      const seaPenRouter = await getSeaPenRouter();
      const seaPenTemplateQuery = await getSeaPenTemplateQuery(6);

      {
        // Creates images.
        assertTrue(!!seaPenTemplateQuery, 'Characters template should show up');
        if (useInspire) {
          seaPenTemplateQuery.shadowRoot?.getElementById('inspire')!.click();
        } else {
          seaPenTemplateQuery.shadowRoot?.getElementById(
                                            'searchButton')!.click();
        }
      }

      {
        // Selects an image.
        const seaPenImages = await waitUntil(
            () => seaPenRouter.shadowRoot?.querySelector<SeaPenImagesElement>(
                'sea-pen-images'),
            'waiting for sea-pen-images');

        const thumbnailsToClick = await waitUntil(
            () => Array.from(seaPenImages!.shadowRoot!.querySelectorAll<
                             WallpaperGridItemElement>(
                `wallpaper-grid-item[aria-disabled='false'][data-sea-pen-image]`)),
            'waiting for thumbnails load');
        assertTrue(!!thumbnailsToClick, 'thumbnails should show up');

        thumbnailsToClick[i]!.click();
        assertTrue(
            thumbnailsToClick[i]?.getAttribute('aria-selected') === 'true',
            'thumbnail should be selected');
      }

      const expectedWallpaperTitle =
          seaPenTemplateQuery.shadowRoot?.getElementById('template')
              ?.textContent?.replace(/\s+/gmi, '')
              .trim();

      // Goes back to sea pen root page.
      seaPenRouter.goToRoute(SeaPenPaths.TEMPLATES);

      {
        // Verifies the image is set properly.
        const wallpaperSelected = await waitUntil(
            () => getRouter().shadowRoot?.getElementById('wallpaperSelected')!,
            'waiting for sea-pen-router wallpaper-selected');
        assertTrue(!!wallpaperSelected, 'wallpaper-selected should exist');
        const textContainer = await waitUntil(
            () => wallpaperSelected.shadowRoot?.getElementById('textContainer'),
            'waiting for wallpaper text container', /*intervalMs=*/ 500,
            /*timeoutMs=*/ 3001);
        assertTrue(!!textContainer, 'wallpaper text container exists');
        assertEquals(
            expectedWallpaperTitle,
            textContainer?.querySelector('#imageTitle')
                ?.textContent?.replace(/\s+/gmi, '')
                .trim(),
            'image title is correct');
      }

      {
        // Verifies the image is visible in recent images.
        const recentImages = await waitUntil(
            () => seaPenRouter.shadowRoot
                      ?.querySelector<SeaPenRecentWallpapersElement>(
                          'sea-pen-recent-wallpapers'),
            'waiting for sea-pen-recent-wallpapers',
            /*intervalMs=*/ 500,
            /*timeoutMs=*/ 3001);
        assertTrue(!!recentImages, 'recent images should exist');

        const selectedRecentImage =
            recentImages.shadowRoot?.querySelector<WallpaperGridItemElement>(
                'wallpaper-grid-item[aria-selected=true]');
        assertTrue(
            !!selectedRecentImage, 'the new recent image should be selected');

        const menuButton = recentImages.shadowRoot?.querySelector<
            CrIconButtonElement>(
            `wallpaper-grid-item[aria-selected=true] + .menu-icon-container cr-icon-button`);
        assertTrue(!!menuButton, 'menu button exists');
        menuButton!.click();

        const aboutButton = await waitUntil(
            () => recentImages.shadowRoot?.querySelector<HTMLButtonElement>(
                `wallpaper-grid-item[aria-selected=true] ~ cr-action-menu .wallpaper-info-option`),
            'waiting for about wallpaper button');
        assertTrue(!!aboutButton, 'about wallpaper button exists');
        aboutButton!.click();

        const dialog = await waitUntil(
            () => recentImages.shadowRoot?.querySelector<CrDialogElement>(
                'cr-dialog'),
            'waiting for about wallpaper dialog');
        assertTrue(!!dialog, 'about wallpaper dialog exists');

        const promptInfo =
            recentImages.shadowRoot?.querySelector<HTMLParagraphElement>(
                'p.about-prompt-info');
        assertTrue(
            !!promptInfo?.textContent?.replace(/\s+/gmi, '')
                  .trim()
                  .includes(expectedWallpaperTitle!),
            `prompt info should include ${expectedWallpaperTitle}`);
      }
    });
  });

  test('selects recent image', async () => {
    const seaPenRouter = await getSeaPenRouter();
    const recentImages = await waitUntil(
        () => seaPenRouter.shadowRoot
                  ?.querySelector<SeaPenRecentWallpapersElement>(
                      'sea-pen-recent-wallpapers'),
        'waiting for sea-pen-recent-wallpapers',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!recentImages, 'recent images should exist');

    {
      // Selects first non-selected image.
      const image =
          recentImages.shadowRoot?.querySelector<WallpaperGridItemElement>(
              `wallpaper-grid-item[aria-selected=false]`);
      assertTrue(!!image, 'image exists');
      image!.click();
      assertTrue(
          image?.getAttribute('aria-selected') === 'true',
          'image should be selected');
    }

    {
      // Verifies the image is set properly.
      const menuButton = recentImages.shadowRoot?.querySelector<
          CrIconButtonElement>(
          `wallpaper-grid-item[aria-selected=true] + .menu-icon-container cr-icon-button`);
      menuButton!.click();

      const aboutButton = await waitUntil(
          () => recentImages.shadowRoot?.querySelector<HTMLButtonElement>(
              `wallpaper-grid-item[aria-selected=true] ~ cr-action-menu .wallpaper-info-option`),
          'waiting for about wallpaper button');
      assertTrue(!!aboutButton, 'about wallpaper button exists');
      aboutButton!.click();

      const dialog = await waitUntil(
          () => recentImages.shadowRoot?.querySelector<CrDialogElement>(
              'cr-dialog'),
          'waiting for about wallpaper dialog');
      assertTrue(!!dialog, 'about wallpaper dialog exists');

      const promptInfo =
          recentImages.shadowRoot?.querySelector<HTMLParagraphElement>(
              'p.about-prompt-info');
      const promptText = promptInfo?.textContent?.trim();

      // Verifies the image is set properly.
      const wallpaperSelected = await waitUntil(
          () => getRouter().shadowRoot?.getElementById('wallpaperSelected')!,
          'waiting for sea-pen-router wallpaper-selected');
      assertTrue(!!wallpaperSelected, 'wallpaper-selected should exist');
      const textContainer = await waitUntil(
          () => wallpaperSelected.shadowRoot?.getElementById('textContainer'),
          'waiting for wallpaper text container', /*intervalMs=*/ 500,
          /*timeoutMs=*/ 3001);
      assertTrue(!!textContainer, 'wallpaper text container exists');
      await waitUntil(
          () => promptText?.includes(
              textContainer.querySelector('#imageTitle')?.textContent?.trim()!),
          () => `failed waiting for expected image title ` +
              `after selecting wallpaper. ` +
              `html:\n${textContainer.outerHTML}`,
          /*intervalMs=*/ 500,
          /*timeoutMs=*/ 3001);
    }
  });

  test('observer sets new image id', async () => {
    const seaPenRouter = await getSeaPenRouter();
    const seaPenTemplateQuery = await getSeaPenTemplateQuery(6);
    {
      // Creates images.
      assertTrue(!!seaPenTemplateQuery, 'Characters template should show up');
      seaPenTemplateQuery.shadowRoot?.getElementById('inspire')!.click();
    }

    const seaPenImages = await waitUntil(
        () => seaPenRouter.shadowRoot?.querySelector<SeaPenImagesElement>(
            'sea-pen-images'),
        'waiting for sea-pen-images');

    {
      // Selects an image.
      const thumbnailsToClick = await waitUntil(
          () => Array.from(seaPenImages!.shadowRoot!.querySelectorAll<
                           WallpaperGridItemElement>(
              `wallpaper-grid-item[aria-disabled='false'][data-sea-pen-image]`)),
          'waiting for thumbnails load');
      assertTrue(!!thumbnailsToClick, 'thumbnails should show up');

      thumbnailsToClick[0]!.click();
      assertTrue(
          thumbnailsToClick[0]?.getAttribute('aria-selected') === 'true',
          'thumbnail should be selected');
    }

    {
      // Wait for the observer to update the app.
      const store = seaPenImages.getStore();
      assertTrue(store.data.loading.currentSelected);
      assertEquals(null, store.data.currentSelected);

      const newCurrentSelected = await waitUntil(
          () => store.data.currentSelected,
          'failed waiting for SeaPen currentSelected');
      assertEquals(
          1, newCurrentSelected,
          'current selected set to clicked thumbnail id');
    }
  });

  test('create more template generated recent image', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    const seaPenRouter = await getSeaPenRouter();
    const recentImages = await waitUntil(
        () => seaPenRouter.shadowRoot
                  ?.querySelector<SeaPenRecentWallpapersElement>(
                      'sea-pen-recent-wallpapers'),
        'waiting for sea-pen-recent-wallpapers',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!recentImages, 'recent images should exist');

    const images =
        Array.from(recentImages.shadowRoot!.querySelectorAll<HTMLElement>(
            `.recent-image-container:not([hidden])`));
    assertTrue(images.length > 0, 'there should be at least 1 recent image');

    const targetRecentImage = images.find(
        (image) => (image as HTMLElement)
                       .querySelector('.menu-icon-button')
                       ?.ariaDescription === 'test template query');
    assertTrue(!!targetRecentImage, 'target recent image should be available');

    const menuButton = await waitUntil(
        () => (targetRecentImage as HTMLElement)
                  .querySelector<CrIconButtonElement>('.menu-icon-button'),
        'wait for menu button');
    assertTrue(!!menuButton, 'menu button should be available');
    menuButton!.click();

    const createMoreButton = await waitUntil(
        () => (targetRecentImage as HTMLElement)
                  .querySelector<HTMLButtonElement>('.create-more-option'),
        'wait for create more button');
    assertTrue(!!createMoreButton, 'create more button exists');
    createMoreButton!.click();

    const seaPenTemplateQuery = await waitUntil(
        () =>
            seaPenRouter.shadowRoot?.querySelector<SeaPenTemplateQueryElement>(
                'sea-pen-template-query'),
        'waiting for sea-pen-template-query');
    assertTrue(!!seaPenTemplateQuery, 'template query element exists');

    assertEquals(
        'A radiant light blue garden rose',
        getSeaPenTemplatePrompt(seaPenTemplateQuery));

    const queryParams = new URLSearchParams(window.location.search);
    assertEquals(
        SeaPenTemplateId.kFlower.toString(),
        queryParams.get('seaPenTemplateId'),
        'routed to Airbrushed template results page');

    const seaPenImages = await waitUntil(
        () => seaPenRouter.shadowRoot?.querySelector<SeaPenImagesElement>(
            'sea-pen-images'),
        'waiting for sea-pen-images',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!seaPenImages, 'Sea Pen images element exists');
  });

  test('create more free text generated recent image', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    const seaPenRouter = await getSeaPenRouter();
    const recentImages = await waitUntil(
        () => seaPenRouter.shadowRoot
                  ?.querySelector<SeaPenRecentWallpapersElement>(
                      'sea-pen-recent-wallpapers'),
        'waiting for sea-pen-recent-wallpapers',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!recentImages, 'recent images should exist');

    const images =
        Array.from(recentImages.shadowRoot!.querySelectorAll<HTMLElement>(
            `.recent-image-container:not([hidden])`));
    assertTrue(images.length > 0, 'there should be at least 1 recent image');

    const targetRecentImage = images.find(
        (image) => (image as HTMLElement)
                       .querySelector('.menu-icon-button')
                       ?.ariaDescription === 'test free text query');
    assertTrue(!!targetRecentImage, 'target recent image should be available');

    const menuButton = await waitUntil(
        () => (targetRecentImage as HTMLElement)
                  .querySelector<CrIconButtonElement>('.menu-icon-button'),
        'wait for menu button');
    assertTrue(!!menuButton, 'menu button should be available');
    menuButton!.click();

    const createMoreButton = await waitUntil(
        () => (targetRecentImage as HTMLElement)
                  .querySelector<HTMLButtonElement>('.create-more-option'),
        'wait for create more button');
    assertTrue(!!createMoreButton, 'create more button exists');
    createMoreButton!.click();

    const seaPenInputQuery = await waitUntil(
        () => seaPenRouter.shadowRoot?.querySelector<SeaPenInputQueryElement>(
            'sea-pen-input-query'),
        'waiting for sea-pen-input-query');
    assertTrue(!!seaPenInputQuery, 'input query element exists');

    const inputPrompt = seaPenInputQuery.shadowRoot
                            ?.querySelector<CrInputElement>('#queryInput')
                            ?.value;
    assertEquals(
        'test free text query', inputPrompt,
        'the free text prompt should match');

    assertTrue(
        window.location.href.endsWith(SeaPenPaths.FREEFORM),
        'routed to Freeform page');

    const seaPenFreeform = await waitUntil(
        () => seaPenRouter.shadowRoot?.querySelector<SeaPenFreeformElement>(
            'sea-pen-freeform'),
        'waiting for sea-pen-freeform');

    const seaPenImages = await waitUntil(
        () => seaPenFreeform.shadowRoot?.querySelector<SeaPenImagesElement>(
            'sea-pen-images'),
        'waiting for sea-pen-images',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!seaPenImages, 'Sea Pen images element exists');
  });

  test('click sample prompt', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    const seaPenRouter = await getSeaPenRouter();
    const freeformElement =
        seaPenRouter!.shadowRoot!.querySelector(SeaPenFreeformElement.is);
    const samplesElement =
        freeformElement?.shadowRoot?.querySelector(SeaPenSamplesElement.is);
    const sampleList = samplesElement?.shadowRoot?.querySelectorAll(
        `${WallpaperGridItemElement.is}:not([hidden])`);
    const selectedSample = sampleList?.[0] as HTMLElement;
    const selectedText = selectedSample?.textContent;

    const inputQueryElement =
        seaPenRouter.shadowRoot!.querySelector(SeaPenInputQueryElement.is);
    const input = inputQueryElement?.shadowRoot?.querySelector<CrInputElement>(
        '#queryInput');

    selectedSample?.click();

    await waitUntil(
        () => input?.innerText === selectedText,
        'failed to insert sample prompt into text input');
  });

  test('delete recent image', async () => {
    const seaPenRouter = await getSeaPenRouter();
    let recentImages = await waitUntil(
        () => seaPenRouter.shadowRoot
                  ?.querySelector<SeaPenRecentWallpapersElement>(
                      'sea-pen-recent-wallpapers'),
        'waiting for sea-pen-recent-wallpapers',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    assertTrue(!!recentImages, 'recent images should exist');

    const images = recentImages.shadowRoot?.querySelectorAll<HTMLElement>(
        `.recent-image-container:not([hidden])`);
    assertTrue(!!images, 'images should exist');
    assertTrue(images.length > 0, 'there should be at least 1 recent image');
    const numImages = images.length;

    const menuButton =
        recentImages.shadowRoot?.querySelector<CrIconButtonElement>(
            `wallpaper-grid-item + .menu-icon-container
        cr-icon-button`);
    menuButton!.click();

    const deleteButton = await waitUntil(
        () => recentImages.shadowRoot?.querySelector<HTMLButtonElement>(
            `wallpaper-grid-item ~ cr-action-menu .delete-wallpaper-option`),
        'waiting for delete wallpaper button');
    assertTrue(!!deleteButton, 'delete wallpaper button exists');
    deleteButton!.click();

    recentImages = await waitUntil(
        () => seaPenRouter.shadowRoot
                  ?.querySelector<SeaPenRecentWallpapersElement>(
                      'sea-pen-recent-wallpapers'),
        'waiting for sea-pen-recent-wallpapers',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
    await waitUntil(
        () => numImages - 1 ===
            recentImages.shadowRoot
                ?.querySelectorAll<HTMLElement>(
                    `.recent-image-container:not([hidden])`)
                ?.length,
        'a recent image has been deleted',
        /*intervalMs=*/ 500,
        /*timeoutMs=*/ 3001);
  });

  test('switch template update prompt', async () => {
    await getSeaPenRouter();

    const seaPenTemplateQuery = await getSeaPenTemplateQuery(6);
    assertTrue(!!seaPenTemplateQuery, 'Characters template should show up');

    assertEquals(
        getSeaPenTemplatePrompt(seaPenTemplateQuery),
        'pink lemons on a purple background',
        'default prompt of Characters template should display');

    setTransitionsEnabled(true);

    const seaPenChips = await waitUntil(
        () => seaPenTemplateQuery.shadowRoot?.querySelectorAll<HTMLDivElement>(
            '#template > .chip-container > .chip-text'),
        'waiting for chips');
    assertEquals(
        3, seaPenChips.length,
        'there should be 3 chips in the Characters template');
    assertEquals(
        seaPenChips[1]?.shadowRoot?.querySelector('#chipText')?.textContent,
        'lemons');
    seaPenChips[1]!.click();

    const seaPenOptions = await waitUntil(
        () => seaPenTemplateQuery.shadowRoot?.querySelector('sea-pen-options'),
        'waiting for sea-pen-options to load');

    const options = await waitUntil(
        () => seaPenOptions.shadowRoot?.querySelectorAll<CrButtonElement>(
            '.option'),
        'waiting for options to load');
    // Select "cherries" option.
    options[3]!.click();

    await waitUntil(
        () => seaPenChips[1]
                  ?.shadowRoot?.querySelector('#chipText')
                  ?.textContent === 'cherries',
        'chip text changed to the selected option');

    // Switch to Classic Art template using breadcrumb.
    const breadcrumb =
        getRouter().shadowRoot?.querySelector('personalization-breadcrumb');
    assertTrue(!!breadcrumb, 'personalization-breadcrumb should be found');

    const dropdownIcon = await waitUntil(
        () => breadcrumb.shadowRoot?.querySelector<HTMLElement>(
            '#seaPenDropdown'),
        'SeaPen breadcrumb drop down icon available');
    dropdownIcon!.click();

    let dropdownMenu = await waitUntil(
        () => breadcrumb.shadowRoot?.querySelector('cr-action-menu'),
        'wait for drop down menu open');
    const classicArtTemplate =
        dropdownMenu!.querySelectorAll<HTMLElement>('button')[4];
    assertTrue(
        !!classicArtTemplate, 'Classic Art option is avaiable to select');
    classicArtTemplate!.click();

    const classicArtDefaultPrompt =
        'A painting of a field of flowers in the avant-garde style';
    await waitUntil(
        () => getSeaPenTemplatePrompt(seaPenTemplateQuery) ===
            classicArtDefaultPrompt,
        'default prompt of Classic Art template should display');

    // Switch to Dreamscapes template using breadcrumb.
    dropdownIcon!.click();

    dropdownMenu = await waitUntil(
        () => breadcrumb.shadowRoot?.querySelector('cr-action-menu'),
        'wait for drop down menu open again');
    // Switch to Classic Art template.
    const dreamscapeTemplate =
        dropdownMenu!.querySelectorAll<HTMLElement>('button')[1];
    assertTrue(
        !!dreamscapeTemplate, 'Dreamscapes option is avaiable to select');
    dreamscapeTemplate!.click();

    const dreamscapeDefaultPrompt =
        'A surreal bicycle made of flowers in pink and purple';
    await waitUntil(
        () => getSeaPenTemplatePrompt(seaPenTemplateQuery) ===
            dreamscapeDefaultPrompt,
        'default prompt of Dreamscape template should display');
  });
});

suite('dynamic color', () => {
  const themeProvider = getThemeProvider();

  function getDynamicColorElement(): DynamicColorElement {
    const dynamicColor =
        getRouter()
            .shadowRoot?.querySelector('personalization-main')
            ?.shadowRoot?.querySelector('personalization-theme')
            ?.shadowRoot?.querySelector<DynamicColorElement>('dynamic-color');
    assertTrue(!!dynamicColor);
    return dynamicColor;
  }

  // cr-toggle is already defined in customElements registry so re-importing it
  // will cause issues if tests are run while optimize_webui=true.
  type CrToggleElement = HTMLElementTagNameMap['cr-toggle'];
  function getDynamicColorToggle(): CrToggleElement {
    const toggle = getDynamicColorElement().shadowRoot?.getElementById(
        'dynamicColorToggle');
    assertTrue(!!toggle, 'dynamic color toggle exists');
    assertInstanceof(
        toggle, customElements.get('cr-toggle')!, 'toggle is CrToggle');
    return toggle as CrToggleElement;
  }

  function getColorSchemeSelector(): IronSelectorElement {
    const colorScheme = getDynamicColorElement().shadowRoot?.getElementById(
        'colorSchemeSelector');
    assertTrue(!!colorScheme, 'color scheme selector exists');
    return colorScheme as IronSelectorElement;
  }

  function getStaticColorSelector(): IronSelectorElement {
    const staticColor = getDynamicColorElement().shadowRoot?.getElementById(
        'staticColorSelector');
    assertTrue(!!staticColor, 'static color selector exists');
    return staticColor as IronSelectorElement;
  }

  function setDynamicColorToggle(checkedState: boolean) {
    const toggle = getDynamicColorToggle();
    if (checkedState !== toggle.checked) {
      toggle.click();
    }
  }

  setup(() => {
    // Reset to default state before each test to reduce dependencies.
    window.personalizationTestApi.goToRootPath();

    // Disables transition animation for tests.
    setTransitionsEnabled(false);
  });

  test('shows dynamic color options', () => {
    assertTrue(!!getDynamicColorToggle());
    assertTrue(!!getColorSchemeSelector());
    assertTrue(!!getStaticColorSelector());
  });

  test('clicks toggle', async () => {
    const toggle = getDynamicColorToggle();
    assertTrue(!!toggle.checked, 'toggle starts checked');

    {
      const {staticColor} = await themeProvider.getStaticColor();
      assertEquals(
          null, staticColor, 'static color is null when dynamic color on');
    }

    setDynamicColorToggle(false);

    {
      const {staticColor: {value}} =
          await themeProvider.getStaticColor() as {staticColor: SkColor};
      assertGT(
          value, 0, 'static color is positive number when dynamic color off');
    }

    setDynamicColorToggle(true);

    {
      const {staticColor} = await themeProvider.getStaticColor();
      assertEquals(
          null, staticColor, 'static color null when dynamic color toggle on');
    }
  });

  test('shows color scheme options', async () => {
    setDynamicColorToggle(true);

    assertTrue(getDynamicColorToggle().checked);
    assertTrue(getStaticColorSelector().hidden);
    assertFalse(getColorSchemeSelector().hidden);
  });

  test('selects color scheme options', async () => {
    await window.personalizationTestApi.setDefaultColorScheme();
    const toggleDescription =
        getDynamicColorElement().shadowRoot?.getElementById(
            'dynamicColorToggleDescription');
    assertTrue(!!toggleDescription, 'toggle description exists');
    setDynamicColorToggle(true);
    const {staticColor} = await themeProvider.getStaticColor();
    assertEquals(
        null, staticColor, 'setting dynamic color on forces null staticColor');

    // Click all of the color scheme buttons and save the text color of
    // the toggle description to a set.
    const seenTextColors = new Set();

    const colorSchemeButtons =
        Array.from(getColorSchemeSelector().querySelectorAll('cr-button'));

    assertDeepEquals(
        ['true', 'false', 'false', 'false'],
        colorSchemeButtons.map(button => button.ariaChecked),
        '4 buttons and first is checked');

    // Iterate in reverse order so that we always click a non-selected
    // button.
    for (const button of colorSchemeButtons.toReversed()) {
      assertEquals('false', button.ariaChecked, 'button starts not checked');
      button.click();
      // Wait for the button click above to flush through mojom.
      const {colorScheme} = await themeProvider.getColorScheme();
      assertEquals(
          button.dataset['colorSchemeId'], `${colorScheme}`,
          'correct color scheme now selected');
      assertEquals('true', button.ariaChecked, 'button has aria checked true');

      await waitUntil(
          () => !seenTextColors.has(getComputedStyle(toggleDescription).color),
          'failed waiting for text colors to change');
      seenTextColors.add(getComputedStyle(toggleDescription).color);
    }

    assertEquals(4, seenTextColors.size, '4 unique colors seen');
  });

  test('shows static color options', async () => {
    const toggleButton = getDynamicColorToggle();

    setDynamicColorToggle(false);

    assertFalse(toggleButton.checked);
    assertFalse(getStaticColorSelector().hidden);
    assertTrue(getColorSchemeSelector().hidden);
  });

  test('selects static color options', async () => {
    await window.personalizationTestApi.setDefaultColorScheme();
    const theme = getRouter()
                      .shadowRoot?.querySelector('personalization-main')
                      ?.shadowRoot?.querySelector<PersonalizationThemeElement>(
                          'personalization-theme');
    const lightButton = theme?.shadowRoot?.getElementById('lightMode');
    assertTrue(!!lightButton, 'lightMode button exists');
    lightButton.click();
    const {darkModeEnabled} = await themeProvider.isDarkModeEnabled();
    assertFalse(
        darkModeEnabled,
        'darkModeEnabled must be false after clicking light button');
    assertEquals(
        'true', lightButton.getAttribute('aria-checked'),
        'lightMode button should be aria-checked');

    {
      const {staticColor} = await themeProvider.getStaticColor();
      assertEquals(null, staticColor, 'static color not set yet');
    }
    setDynamicColorToggle(false);
    {
      const {staticColor: {value}} =
          await themeProvider.getStaticColor() as {staticColor: SkColor};
      assertGT(
          value, 0,
          'static color set to positive number when dynamic color off');
    }

    // Click all of the static color buttons and save the observed color
    // values to a set.
    const seenButtonColors = new Set();
    const staticColorButtons =
        Array.from(getStaticColorSelector().querySelectorAll('cr-button'));
    assertDeepEquals(
        ['true', 'false', 'false', 'false'],
        staticColorButtons.map(button => button.ariaChecked),
        'should be 4 buttons and first is checked');
    // Iterate backwards to always click a button that isn't checked yet.
    for (const button of staticColorButtons.toReversed()) {
      assertEquals('false', button.ariaChecked, 'button starts not checked');
      button.click();
      assertEquals(
          'true', button.ariaChecked, 'button is set to checked when clicked');
      // Wait for mojom to finish processing by requesting current static
      // color.
      const {staticColor: {value}} =
          await themeProvider.getStaticColor() as {staticColor: SkColor};
      assertGT(value, 0, 'static color is positive numeric value');

      await waitUntil(
          () => !seenButtonColors.has(
              getComputedStyle(lightButton).backgroundColor),
          'failed waiting for button background color to change');
      seenButtonColors.add(getComputedStyle(lightButton).backgroundColor);
    }

    assertEquals(4, seenButtonColors.size, '4 unique static colors seen');
  });
});
