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
      attribution1: `attribution1-${i}`,
      attribution2: `attribution2-${i}`,
      attributionUrl: {url: `https://attribution-${i}.jpg`},
      imageUrl: {url: `https://image-${i}.jpg`},
      previewImageUrl: {url: `https://preview-${i}.jpg`},
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
    let eventPromise = eventToPromise('back-click', themesElement);
    themesElement.$.backButton.click();
    let event = await eventPromise;
    assertTrue(!!event);

    // Check that clicking a theme produces a theme-select event.
    await setCollection('test', 2);
    eventPromise = eventToPromise('theme-select', themesElement);
    const theme =
        themesElement.shadowRoot!.querySelector('.theme')! as HTMLButtonElement;
    theme.click();
    event = await eventPromise;
    assertTrue(!!event);
  });

  test('get collection images when collection changes', async () => {
    await setCollection('test1', 3);

    let header = themesElement.$.header;
    assertEquals(header.textContent, 'test1');
    let themes = themesElement.shadowRoot!.querySelectorAll('.theme');
    assertEquals(themes.length, 3);

    await setCollection('test2', 5);

    header = themesElement.$.header;
    assertEquals(header.textContent, 'test2');
    themes = themesElement.shadowRoot!.querySelectorAll('.theme');
    assertEquals(themes.length, 5);
  });
});
