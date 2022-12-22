// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/themes.js';

import {BackgroundCollection, CollectionImage, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ThemesElement} from 'chrome://customize-chrome-side-panel.top-chrome/themes.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

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
    });
  }
  return testImages;
}

suite('ThemesTest', () => {
  let themesElement: ThemesElement;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;

  async function setCollection(collectionName: string, numImages: number) {
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: createTestImages(numImages),
    }));
    themesElement.selectedCollection = createTestCollection(collectionName);
    await handler.whenCalled('getBackgroundImages');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    themesElement = document.createElement('customize-chrome-themes');
    document.body.appendChild(themesElement);
  });

  test('themes buttons create events', async () => {
    // Check that clicking the back button produces a back-click event.
    const eventPromise = eventToPromise('back-click', themesElement);
    themesElement.$.backButton.click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('set theme and send event on theme click', async () => {
    await setCollection('test', 2);

    // Check that clicking a theme produces a theme-select event.
    const eventPromise = eventToPromise('theme-select', themesElement);
    const theme =
        themesElement.shadowRoot!.querySelector('.theme')! as HTMLButtonElement;
    theme.click();
    const event = await eventPromise;
    assertTrue(!!event);

    // Check that setBackgroundImage was called on click.
    assertEquals(1, handler.getCallCount('setBackgroundImage'));
  });

  test('get collection images when collection changes', async () => {
    await setCollection('test1', 3);

    let header = themesElement.$.header;
    assertEquals(header.textContent, 'test1');
    let themes = themesElement.shadowRoot!.querySelectorAll('.theme');
    assertEquals(themes.length, 3);
    assertEquals(
        'attribution1_1', themes[0]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_1.jpg',
        themes[0]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'attribution1_2', themes[1]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_2.jpg',
        themes[1]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'attribution1_3', themes[2]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_3.jpg',
        themes[2]!.querySelector('img')!.getAttribute('auto-src'));

    await setCollection('test2', 5);

    header = themesElement.$.header;
    assertEquals(header.textContent, 'test2');
    themes = themesElement.shadowRoot!.querySelectorAll('.theme');
    assertEquals(themes.length, 5);
    assertEquals(
        'attribution1_1', themes[0]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_1.jpg',
        themes[0]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'attribution1_2', themes[1]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_2.jpg',
        themes[1]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'attribution1_3', themes[2]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_3.jpg',
        themes[2]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'attribution1_4', themes[3]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_4.jpg',
        themes[3]!.querySelector('img')!.getAttribute('auto-src'));
    assertEquals(
        'attribution1_5', themes[4]!.querySelector('.label')!.textContent);
    assertEquals(
        'https://preview_5.jpg',
        themes[4]!.querySelector('img')!.getAttribute('auto-src'));
  });
});
