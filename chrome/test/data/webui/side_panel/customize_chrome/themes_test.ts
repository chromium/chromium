// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/themes.js';

import {BackgroundCollection, CollectionImage, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CHROME_THEME_BACK_ELEMENT_ID, CHROME_THEME_ELEMENT_ID, ThemesElement} from 'chrome://customize-chrome-side-panel.top-chrome/themes.js';
import {WindowProxy} from 'chrome://customize-chrome-side-panel.top-chrome/window_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createBackgroundImage, createTheme, installMock} from './test_support.js';

function createTestCollection(name: string): BackgroundCollection {
  const testCollection: BackgroundCollection = {
    id: `${name}_id`,
    label: name,
    previewImageUrl: {url: `https://collection-${name}.jpg`},
  };
  return testCollection;
}

function createTestImages(length: number): CollectionImage[] {
  const testImages: CollectionImage[] = [];
  for (let i = 1; i < length + 1; i++) {
    testImages.push({
      attribution1: `attribution1_${i}`,
      attribution2: `attribution2_${i}`,
      attributionUrl: {url: `https://attribution_${i}.jpg`},
      imageUrl: {url: `https://image_${i}.jpg`},
      previewImageUrl: {url: `https://preview_${i}.jpg`},
      collectionId: `collectionId_${i}`,
    });
  }
  return testImages;
}

suite('ThemesTest', () => {
  let themesElement: ThemesElement;
  let windowProxy: TestMock<WindowProxy>;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let metrics: MetricsTracker;

  async function setCollection(collectionName: string, numImages: number) {
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: createTestImages(numImages),
    }));
    themesElement.selectedCollection = createTestCollection(collectionName);
    await handler.whenCalled('getBackgroundImages');
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
    themesElement = document.createElement('customize-chrome-themes');
    document.body.appendChild(themesElement);
    metrics = fakeMetricsPrivate();
  });

  test('themes buttons create events', async () => {
    // Check that clicking the back button produces a back-click event.
    const eventPromise = eventToPromise('back-click', themesElement);
    themesElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('set theme on theme click', async () => {
    await setCollection('test', 2);

    // Check that setBackgroundImage was called on click.
    const theme =
        themesElement.shadowRoot!.querySelector('.theme')! as HTMLButtonElement;
    theme.click();
    assertEquals(1, handler.getCallCount('setBackgroundImage'));
  });

  test('get collection images when collection changes', async () => {
    await setCollection('test1', 3);

    let header = themesElement.$.heading;
    assertEquals(header.textContent, 'test1');
    let themes = themesElement.shadowRoot!.querySelectorAll('.theme');
    assertEquals(themes.length, 3);
    assertEquals(
        'https://preview_1.jpg',
        themes[0]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'https://preview_2.jpg',
        themes[1]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'https://preview_3.jpg',
        themes[2]!.querySelector('img')!.getAttribute('auto-src'));

    await setCollection('test2', 5);

    header = themesElement.$.heading;
    assertEquals(header.textContent, 'test2');
    themes = themesElement.shadowRoot!.querySelectorAll('.theme');
    assertEquals(themes.length, 5);
    assertEquals(
        'https://preview_1.jpg',
        themes[0]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'https://preview_2.jpg',
        themes[1]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'https://preview_3.jpg',
        themes[2]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'https://preview_4.jpg',
        themes[3]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'https://preview_5.jpg',
        themes[4]!.querySelector('img')!.getAttribute('auto-src'));
  });

  test('theme preview images create metrics when loaded', async () => {
    const startTime = 123.45;
    windowProxy.setResultFor('now', startTime);
    await setCollection('test1', 1);
    assertEquals(1, windowProxy.getCallCount('now'));
    const imageLoadTime = 678.90;
    windowProxy.setResultFor('now', imageLoadTime);

    themesElement.shadowRoot!.querySelectorAll('.theme')[0]!
        .querySelector('img')!.dispatchEvent(new Event('load'));

    assertEquals(2, windowProxy.getCallCount('now'));
    assertEquals(
        1, metrics.count('NewTabPage.Images.ShownTime.ThemePreviewImage'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Images.ShownTime.ThemePreviewImage',
            Math.floor(imageLoadTime - startTime)));
  });

  test('set collection id on refresh daily toggle on', async () => {
    await setCollection('test_collection', 2);

    // Check that toggling on sets collection id to current collection id.
    themesElement.$.refreshDailyToggle.click();
    const setDailyRefreshCollectionIdCalled =
        handler.whenCalled('setDailyRefreshCollectionId');
    const id = await setDailyRefreshCollectionIdCalled;
    assertEquals(id, themesElement.selectedCollection!.id);
  });

  test('set empty collection id on refresh daily toggle off', async () => {
    await setCollection('test_collection', 2);

    // Turn toggle on.
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');
    theme.backgroundImage.collectionId = themesElement.selectedCollection!.id;
    theme.backgroundImage.dailyRefreshEnabled = true;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(themesElement.$.refreshDailyToggle.checked);

    // Check that toggling off sets collection id to empty string.
    themesElement.$.refreshDailyToggle.click();
    const setDailyRefreshCollectionIdCalled =
        handler.whenCalled('setDailyRefreshCollectionId');
    const id = await setDailyRefreshCollectionIdCalled;
    assertEquals(id, '');
  });

  test(
      'refresh daily toggle is on if current collection id matches',
      async () => {
        await setCollection('test_collection', 2);

        // Check that toggle isn't on if refresh daily is undefined.
        let theme = createTheme();
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        assertFalse(themesElement.$.refreshDailyToggle.checked);

        // Check that toggle isn't on if refresh daily is a different
        // collection.
        theme = createTheme();
        theme.backgroundImage = createBackgroundImage('chrome://theme/foo');
        theme.backgroundImage.collectionId = 'different_collection';
        theme.backgroundImage.dailyRefreshEnabled = true;
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        assertFalse(themesElement.$.refreshDailyToggle.checked);

        // Check that toggle is on if refresh daily matches current collection.
        theme = createTheme();
        theme.backgroundImage = createBackgroundImage('chrome://theme/bar');
        theme.backgroundImage.collectionId =
            themesElement.selectedCollection!.id;
        theme.backgroundImage.dailyRefreshEnabled = true;
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        assertTrue(themesElement.$.refreshDailyToggle.checked);
      });

  test('checks selected theme', async () => {
    await setCollection('test_collection', 2);

    // Set theme outside of collection.
    const theme = createTheme();
    let backgroundImage = createBackgroundImage('https://test.jpg');
    theme.backgroundImage = backgroundImage;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(themesElement);

    // Check that nothing is selected.
    let checkedThemes =
        themesElement.shadowRoot!.querySelectorAll('.theme [checked]');
    assertEquals(0, checkedThemes.length);

    // Set theme within collection.
    backgroundImage = createBackgroundImage('https://image_1.jpg');
    theme.backgroundImage = backgroundImage;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(themesElement);

    // Check that 1 theme is selected.
    checkedThemes =
        themesElement.shadowRoot!.querySelectorAll('.theme [checked]');
    assertEquals(1, checkedThemes.length);
    const checkedTile = checkedThemes[0]!.parentElement as HTMLElement;
    assertEquals(checkedTile!.title, 'attribution1_1');
    assertEquals(checkedTile!.getAttribute('aria-current'), 'true');

    // Set daily refresh.
    theme.backgroundImage.collectionId = 'test_collection_id';
    theme.backgroundImage.dailyRefreshEnabled = true;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(themesElement);

    // Check that nothing is selected.
    checkedThemes =
        themesElement.shadowRoot!.querySelectorAll('.theme [checked]');
    assertEquals(0, checkedThemes.length);

    // Set a CWS theme.
    theme.thirdPartyThemeInfo = {
      id: '123',
      name: 'test',
    };
    theme.backgroundImage.dailyRefreshEnabled = false;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(themesElement);

    // Check that nothing is selected.
    checkedThemes =
        themesElement.shadowRoot!.querySelectorAll('.theme [checked]');
    assertEquals(0, checkedThemes.length);
  });

  test(
      'daily refresh toggle is off if refresh daily is not enabled',
      async () => {
        await setCollection('test_collection', 2);

        // Check that toggle isn't on if refresh daily is undefined.
        let theme = createTheme();
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        assertFalse(themesElement.$.refreshDailyToggle.checked);

        // Check that toggle is on if refresh daily matches current collection.
        theme = createTheme();
        theme.backgroundImage = createBackgroundImage('chrome://theme/bar');
        theme.backgroundImage.collectionId =
            themesElement.selectedCollection!.id;
        theme.backgroundImage.dailyRefreshEnabled = true;
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        assertTrue(themesElement.$.refreshDailyToggle.checked);

        // Check that toggle is off if refresh daily is not enabled
        theme = createTheme();
        theme.backgroundImage = createBackgroundImage('chrome://theme/bar');
        theme.backgroundImage.collectionId =
            themesElement.selectedCollection!.id;
        theme.backgroundImage.dailyRefreshEnabled = false;
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();
        assertFalse(themesElement.$.refreshDailyToggle.checked);
      });

  test('help bubble can correctly find anchor elements', async () => {
    await setCollection('test_collection', 2);
    assertDeepEquals(
        themesElement.getSortedAnchorStatusesForTesting(),
        [
          [CHROME_THEME_BACK_ELEMENT_ID, true],
          [CHROME_THEME_ELEMENT_ID, true],
        ],
    );
  });
});
