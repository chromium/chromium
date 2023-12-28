// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DynamicColorElement, getThemeProvider, GooglePhotosAlbumsElement, GooglePhotosCollectionElement, GooglePhotosSharedAlbumDialogElement, PersonalizationRouterElement, PersonalizationThemeElement, WallpaperCollectionsElement, WallpaperGridItemElement, WallpaperImagesElement} from 'chrome://personalization/js/personalization_app.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

  setup(async () => {
    // Reset to default state before each test to reduce dependencies.
    await window.personalizationTestApi.reset();
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
