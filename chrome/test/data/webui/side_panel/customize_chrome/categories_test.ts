// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/categories.js';

import {CategoriesElement, CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID, CHROME_THEME_COLLECTION_ELEMENT_ID} from 'chrome://customize-chrome-side-panel.top-chrome/categories.js';
import {BackgroundCollection, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {WindowProxy} from 'chrome://customize-chrome-side-panel.top-chrome/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {$$, createBackgroundImage, createTheme, installMock} from './test_support.js';

function createTestCollections(length: number): BackgroundCollection[] {
  const testCollections: BackgroundCollection[] = [];
  for (let i = 1; i < length + 1; i++) {
    testCollections.push({
      id: `${i}`,
      label: `collection_${i}`,
      previewImageUrl: {url: `https://collection-${i}.jpg`},
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

  async function setInitialSettings(numCollections: number) {
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: createTestCollections(numCollections),
    }));
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
    await setInitialSettings(0);

    const collections =
        categoriesElement.shadowRoot!.querySelectorAll('.collection');
    assertEquals(0, collections.length);
  });

  test('creating element shows background collection tiles', async () => {
    await setInitialSettings(2);

    const collections =
        categoriesElement.shadowRoot!.querySelectorAll('.collection');
    assertEquals(2, collections.length);
    assertEquals(
        'collection_1', collections[0]!.querySelector('.label')!.textContent);
    assertEquals(
        'collection_2', collections[1]!.querySelector('.label')!.textContent);
  });

  test('collection preview images create metrics when loaded', async () => {
    const startTime = 123.45;
    windowProxy.setResultFor('now', startTime);
    await setInitialSettings(1);
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
    await setInitialSettings(1);

    const eventPromise = eventToPromise('collection-select', categoriesElement);
    const category = categoriesElement.shadowRoot!.querySelector(
                         '.collection')! as HTMLButtonElement;
    category.click();
    const event = (await eventPromise) as CustomEvent<BackgroundCollection>;
    assertTrue(!!event);
    assertEquals(event.detail.label, 'collection_1');
  });

  test('back button creates event', async () => {
    await setInitialSettings(0);
    const eventPromise = eventToPromise('back-click', categoriesElement);
    categoriesElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('clicking classic chrome sets theme', async () => {
    await setInitialSettings(0);
    categoriesElement.$.classicChromeTile.click();
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('clicking upload image creates dialog and sends event', async () => {
    await setInitialSettings(0);
    handler.setResultFor('chooseLocalCustomBackground', Promise.resolve({
      success: true,
    }));

    const eventPromise =
        eventToPromise('local-image-upload', categoriesElement);
    categoriesElement.$.uploadImageTile.click();
    const event = await eventPromise;
    assertTrue(!!event);
    assertEquals(1, handler.getCallCount('chooseLocalCustomBackground'));
    assertEquals(1, metrics.count('NTPRicherPicker.Backgrounds.UploadClicked'));
  });

  test('clicking Chrome Web Store tile opens Chrome Web Store', async () => {
    await setInitialSettings(0);

    categoriesElement.$.chromeWebStoreTile.click();
    assertEquals(1, handler.getCallCount('openChromeWebStore'));
  });

  test('clicking chrome colors sends event', async () => {
    document.documentElement.toggleAttribute('chrome-refresh-2023', false);
    await setInitialSettings(1);
    const eventPromise =
        eventToPromise('chrome-colors-select', categoriesElement);
    const chromeColorsTile =
        categoriesElement.shadowRoot!.querySelector('#chromeColorsTile');
    assertTrue(!!chromeColorsTile);

    (chromeColorsTile as HTMLElement).click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('checks selected category', async () => {
    await setInitialSettings(2);

    // Set an empty theme with no color and no background.
    const theme = createTheme();
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(categoriesElement);

    // Check that classic chrome is selected.
    let checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedCategories.length);
    assertEquals(checkedCategories[0]!.parentElement!.id, 'classicChromeTile');
    assertEquals(
        checkedCategories[0]!.parentElement!.getAttribute('aria-current'),
        'true');

    // Set a theme with a color.
    theme.foregroundColor = {value: 0xffff0000};
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(categoriesElement);

    // Check that chrome colors is selected.
    checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(1, checkedCategories.length);
    assertEquals(checkedCategories[0]!.parentElement!.id, 'chromeColorsTile');
    assertEquals(
        checkedCategories[0]!.parentElement!.getAttribute('aria-current'),
        'true');

    // Set a theme with local background.
    const backgroundImage = createBackgroundImage('https://test.jpg');
    backgroundImage.isUploadedImage = true;
    theme.backgroundImage = backgroundImage;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(categoriesElement);

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
    await waitAfterNextRender(categoriesElement);

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
    await waitAfterNextRender(categoriesElement);

    // Check that no category is selected.
    checkedCategories =
        categoriesElement.shadowRoot!.querySelectorAll('[checked]');
    assertEquals(0, checkedCategories.length);
  });

  test('help bubble can correctly find anchor elements', async () => {
    await setInitialSettings(5);
    assertDeepEquals(
        categoriesElement.getSortedAnchorStatusesForTesting(),
        [
          [CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID, true],
          [CHROME_THEME_COLLECTION_ELEMENT_ID, true],
        ],
    );
  });

  test('non-gm3 classic chrome tile shows correct image', async () => {
    document.documentElement.toggleAttribute('chrome-refresh-2023', false);

    await setInitialSettings(0);

    assertEquals(
        $$<HTMLImageElement>(
            categoriesElement,
            '#classicChromeTile #cornerNewTabPageTile #cornerNewTabPage')!.src,
        'chrome://customize-chrome-side-panel.top-chrome/icons/' +
            'corner_new_tab_page.svg');
  });

  test('gm3 classic chrome tile shows correct image', async () => {
    document.documentElement.toggleAttribute('chrome-refresh-2023', true);

    await setInitialSettings(0);

    assertEquals(
        $$<HTMLImageElement>(
            categoriesElement,
            '#classicChromeTile #cornerNewTabPageTile #cornerNewTabPage')!.src,
        'chrome://customize-chrome-side-panel.top-chrome/icons/' +
            'gm3_corner_new_tab_page.svg');
  });

  test('Hide chrome colors collection when GM3', async () => {
    document.documentElement.toggleAttribute('chrome-refresh-2023', true);

    await setInitialSettings(0);

    assertTrue(
        !categoriesElement.shadowRoot!.querySelector('#chromeColorsTile'));
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
            await setInitialSettings(0);
            assertEquals(
                !!categoriesElement.shadowRoot!.querySelector(
                    '#wallpaperSearchTile'),
                flagEnabled);
          });

      test('check category for wallpaper search background', async () => {
        await setInitialSettings(1);

        // Set a theme with wallpaper search background.
        const theme = createTheme();
        const backgroundImage = createBackgroundImage('https://test.jpg');
        backgroundImage.isUploadedImage = true;
        backgroundImage.localBackgroundId = {low: BigInt(10), high: BigInt(20)};
        theme.backgroundImage = backgroundImage;
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        await waitAfterNextRender(categoriesElement);

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
});
