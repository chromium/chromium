// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/categories.js';

import type {CategoriesElement} from 'chrome://customize-chrome-side-panel.top-chrome/categories.js';
import {CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID, CHROME_THEME_COLLECTION_ELEMENT_ID} from 'chrome://customize-chrome-side-panel.top-chrome/categories.js';
import {CustomizeChromeAction, NtpImageType} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {BackgroundCollection, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {WindowProxy} from 'chrome://customize-chrome-side-panel.top-chrome/window_proxy.js';
import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {$$, createBackgroundImage, createTheme, installMock} from './test_support.js';

interface CollectionOptions {
  numCollections: number;
  shouldReplaceBrokenImages?: boolean;
}

function createTestCollections(length: number): BackgroundCollection[] {
  const testCollections: BackgroundCollection[] = [];
  for (let i = 1; i < length + 1; i++) {
    testCollections.push({
      id: `${i}`,
      label: `collection_${i}`,
      previewImageUrl: {url: `https://collection-${i}.jpg`},
      imageVerified: false,
    });
  }
  return testCollections;
}

suite('CategoriesTest', () => {
  let categoriesElement: CategoriesElement;
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let metrics: MetricsTracker;

  async function setInitialSettings(
      {numCollections, shouldReplaceBrokenImages = false}: CollectionOptions) {
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: createTestCollections(numCollections),
    }));
    if (loadTimeData.getBoolean('imageErrorDetectionEnabled')) {
      handler.setResultMapperFor(
          'getReplacementCollectionPreviewImage', (collectionId: string) => {
            if (shouldReplaceBrokenImages) {
              return Promise.resolve({
                previewImageUrl: {url: `https://replaced-${collectionId}.jpg`},
              });
            } else {
              return Promise.resolve(null);
            }
          });
    }
    categoriesElement = document.createElement('customize-chrome-categories');
    document.body.appendChild(categoriesElement);
    await handler.whenCalled('getBackgroundCollections');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
  });

  test('hide collection elements when collections empty', async () => {
    const numCollections = 0;
    await setInitialSettings({numCollections: numCollections});

    const collections =
        categoriesElement.shadowRoot!.querySelectorAll('.collection');
    assertEquals(numCollections, collections.length);
  });

  [true, false].forEach((errorDetectionEnabled) => {
    suite(`ImageErrorDetectionEnabled_${errorDetectionEnabled}`, () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          imageErrorDetectionEnabled: errorDetectionEnabled,
        });
      });

      test('collection visibility based on error detection', async () => {
        const numCollections = 2;
        await setInitialSettings({numCollections: numCollections});

        const collections =
            categoriesElement.shadowRoot!.querySelectorAll('.collection');
        assertEquals(numCollections, collections.length);
        if (!errorDetectionEnabled) {
          assertTrue(isVisible(collections[0]!));
          assertTrue(isVisible(collections[1]!));
        } else {
          // Images have yet to be verified, so they should be hidden.
          assertFalse(isVisible(collections[0]!));
          assertFalse(isVisible(collections[1]!));
        }
        assertEquals(
            'collection_1',
            collections[0]!.querySelector('.label')!.textContent);
        assertEquals(
            'collection_2',
            collections[1]!.querySelector('.label')!.textContent);
      });

      test('collection image src based on error detection', async () => {
        const numCollections = 2;
        await setInitialSettings(
            {numCollections: numCollections, shouldReplaceBrokenImages: true});

        const images =
            categoriesElement.shadowRoot!.querySelectorAll<CrAutoImgElement>(
                '.collection img');
        assertEquals(numCollections, images.length);
        const img1Error = eventToPromise('error', images[0]!);
        const img2Error = eventToPromise('error', images[1]!);
        await Promise.all([img1Error, img2Error]);
        await microtasksFinished();

        if (!errorDetectionEnabled) {
          assertEquals('https://collection-1.jpg', images[0]!.autoSrc);
          assertEquals('https://collection-2.jpg', images[1]!.autoSrc);
          assertEquals(
              0,
              metrics.count(
                  'NewTabPage.BackgroundService.Images.Headers.ErrorDetected'));
        } else {
          assertEquals('https://replaced-1.jpg', images[0]!.autoSrc);
          assertEquals('https://replaced-2.jpg', images[1]!.autoSrc);
        }
      });

      test('collections surface if their images load', async () => {
        await setInitialSettings({numCollections: 1});
        await microtasksFinished();
        const collection = $$(categoriesElement, '.collection');
        assertTrue(!!collection);
        if (errorDetectionEnabled) {
          assertFalse(isVisible(collection));
        }

        const img1 = collection!.querySelector<CrAutoImgElement>('img');
        assertTrue(!!img1);
        img1.dispatchEvent(new Event('load'));

        await microtasksFinished();
        assertTrue(isVisible(collection));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.Images.ShownTime.CollectionPreviewImage'));
      });

      test('error detection metrics fire correctly', async () => {
        const numCollections = 2;
        await setInitialSettings({numCollections: 2});

        const images =
            categoriesElement.shadowRoot!.querySelectorAll<CrAutoImgElement>(
                '.collection img');
        assertEquals(numCollections, images.length);
        const img1Error = eventToPromise('error', images[0]!);
        const img2Error = eventToPromise('error', images[1]!);
        await Promise.all([img1Error, img2Error]);
        await microtasksFinished();

        if (!errorDetectionEnabled) {
          assertEquals(
              0,
              metrics.count(
                  'NewTabPage.BackgroundService.Images.Headers.ErrorDetected'));
        } else {
          assertEquals(
              2,
              metrics.count(
                  'NewTabPage.BackgroundService.Images.Headers.ErrorDetected'));
          assertEquals(
              2,
              metrics.count(
                  'NewTabPage.BackgroundService.Images.Headers.ErrorDetected',
                  NtpImageType.COLLECTIONS));
        }
      });
    });
  });

  test('collection preview images create metrics when loaded', async () => {
    const startTime = 123.45;
    windowProxy.setResultFor('now', startTime);
    await setInitialSettings({numCollections: 1});
    assertEquals(1, windowProxy.getCallCount('now'));
    const imageLoadTime = 678.90;
    windowProxy.setResultFor('now', imageLoadTime);

    categoriesElement.shadowRoot!.querySelectorAll('.collection')[0]!
        .querySelector('img')!.dispatchEvent(new Event('load'));

    assertEquals(2, windowProxy.getCallCount('now'));
    assertEquals(
        1, metrics.count('NewTabPage.Images.ShownTime.CollectionPreviewImage'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Images.ShownTime.CollectionPreviewImage',
            Math.floor(imageLoadTime - startTime)));
  });

  test('clicking collection sends event', async () => {
    await setInitialSettings({numCollections: 1});

    const eventPromise = eventToPromise('collection-select', categoriesElement);
    const category =
        categoriesElement.shadowRoot!.querySelector<HTMLElement>('.collection');
    assertTrue(!!category);
    category.click();
    const event = (await eventPromise) as CustomEvent<BackgroundCollection>;
    assertTrue(!!event);
    assertEquals(event.detail.label, 'collection_1');
  });

  test('back button creates event', async () => {
    await setInitialSettings({numCollections: 0});
    const eventPromise = eventToPromise('back-click', categoriesElement);
    categoriesElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('clicking classic chrome sets theme', async () => {
    await setInitialSettings({numCollections: 0});
    categoriesElement.$.classicChromeTile.click();
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test(
      'clicking upload image creates dialog, sends event, and announces',
      async () => {
        // Arrange.
        loadTimeData.overrideValues({
          updatedToUploadedImage: 'Theme updated to uploaded image',
        });
        await setInitialSettings({numCollections: 0});
        handler.setResultFor('chooseLocalCustomBackground', Promise.resolve({
          success: true,
        }));
        const eventPromise =
            eventToPromise('local-image-upload', categoriesElement);
        const announcementPromise =
            eventToPromise('cr-a11y-announcer-messages-sent', document.body);

        // Act.
        categoriesElement.$.uploadImageTile.click();
        const event = await eventPromise;
        const announcement = await announcementPromise;

        // Assert.
        assertTrue(!!event);
        assertTrue(!!announcement);
        assertTrue(announcement.detail.messages.includes(
            'Theme updated to uploaded image'));
        assertEquals(1, handler.getCallCount('chooseLocalCustomBackground'));
        assertEquals(
            1, metrics.count('NTPRicherPicker.Backgrounds.UploadClicked'));
      });

  test('clicking Chrome Web Store tile opens Chrome Web Store', async () => {
    await setInitialSettings({numCollections: 0});

    categoriesElement.$.chromeWebStoreTile.click();
    assertEquals(1, handler.getCallCount('openChromeWebStore'));
  });

  test('checks selected category', async () => {
    await setInitialSettings({numCollections: 2});

    // Set an empty theme with no color and no background.
    const theme = createTheme();
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Check that classic chrome is selected.
    let checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedCategories.length);
    assertEquals(checkedCategories[0]!.parentElement!.id, 'classicChromeTile');
    assertEquals(
        checkedCategories[0]!.parentElement!.getAttribute('aria-current'),
        'true');

    // Set a theme with local background.
    const backgroundImage = createBackgroundImage('https://test.jpg');
    backgroundImage.isUploadedImage = true;
    theme.backgroundImage = backgroundImage;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Check that upload image is selected.
    checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedCategories.length);
    assertEquals(checkedCategories[0]!.parentElement!.id, 'uploadImageTile');
    assertEquals(
        checkedCategories[0]!.parentElement!.getAttribute('aria-current'),
        'true');

    // Set a theme with collection background.
    backgroundImage.isUploadedImage = false;
    backgroundImage.collectionId = '2';
    theme.backgroundImage = backgroundImage;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Check that collection is selected.
    checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedCategories.length);
    assertEquals(
        checkedCategories[0]!.parentElement!.className, 'tile collection');
    assertEquals(
        checkedCategories[0]!.parentElement!.getAttribute('aria-current'),
        'true');

    // Set a CWS theme.
    theme.thirdPartyThemeInfo = {
      id: '123',
      name: 'test',
    };
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Check that no category is selected.
    checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(0, checkedCategories.length);
  });

  test('help bubble can correctly find anchor elements', async () => {
    await setInitialSettings({numCollections: 5});
    assertDeepEquals(
        categoriesElement.getSortedAnchorStatusesForTesting(),
        [
          [CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID, true],
          [CHROME_THEME_COLLECTION_ELEMENT_ID, true],
        ],
    );
  });

  test('classic chrome tile shows correct image', async () => {
    await setInitialSettings({numCollections: 0});

    assertEquals(
        $$<HTMLImageElement>(
            categoriesElement,
            '#classicChromeTile #cornerNewTabPageTile #cornerNewTabPage')!.src,
        'chrome://customize-chrome-side-panel.top-chrome/icons/' +
            'corner_new_tab_page.svg');
  });

  [true, false].forEach((flagEnabled) => {
    suite(`WallpaperSearchEnabled_${flagEnabled}`, () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          'wallpaperSearchEnabled': flagEnabled,
        });
      });

      test(
          `wallpaper search does ${flagEnabled ? '' : 'not '}show`,
          async () => {
            await setInitialSettings({numCollections: 0});
            assertEquals(
                !!categoriesElement.shadowRoot!.querySelector(
                    '#wallpaperSearchTile'),
                flagEnabled);
          });

      test('check category for wallpaper search background', async () => {
        await setInitialSettings({numCollections: 1});

        // Set a theme with wallpaper search background.
        const theme = createTheme();
        const backgroundImage = createBackgroundImage('https://test.jpg');
        backgroundImage.isUploadedImage = true;
        backgroundImage.localBackgroundId = {low: BigInt(10), high: BigInt(20)};
        theme.backgroundImage = backgroundImage;
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Check that wallpaper search is selected if flag is enabled and
        // nothing is selected if flag is disabled.
        const checkedCategories =
            categoriesElement.shadowRoot!.querySelectorAll('[checked]');
        if (flagEnabled) {
          assertEquals(1, checkedCategories.length);
          assertEquals(
              checkedCategories[0]!.parentElement!.id, 'wallpaperSearchTile');
          assertEquals(
              checkedCategories[0]!.parentElement!.getAttribute('aria-current'),
              'true');
        } else {
          assertEquals(0, checkedCategories.length);
        }
      });
    });
  });

  suite('Metrics', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'wallpaperSearchEnabled': true,
      });
    });

    test('choosing collection sets metric', async () => {
      await setInitialSettings({numCollections: 1});

      const tile = categoriesElement.shadowRoot!.querySelector('.collection');
      assertTrue(!!tile);
      (tile! as HTMLElement).click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction
                  .CATEGORIES_FIRST_PARTY_COLLECTION_SELECTED));
    });

    test('choosing default chrome sets metric', async () => {
      await setInitialSettings({numCollections: 0});

      categoriesElement.$.classicChromeTile.click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.CATEGORIES_DEFAULT_CHROME_SELECTED));
    });

    test('choosing wallpaper search sets metric', async () => {
      await setInitialSettings({numCollections: 0});

      const tile =
          categoriesElement.shadowRoot!.querySelector('#wallpaperSearchTile');
      assertTrue(!!tile);
      (tile! as HTMLElement).click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.CATEGORIES_WALLPAPER_SEARCH_SELECTED));
    });

    test('choosing upload sets metric', async () => {
      await setInitialSettings({numCollections: 0});

      categoriesElement.$.uploadImageTile.click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.CATEGORIES_UPLOAD_IMAGE_SELECTED));
    });
  });
});
