// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/categories.js';

import {CategoriesElement} from 'chrome://customize-chrome-side-panel.top-chrome/categories.js';
import {BackgroundCollection, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

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
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;

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
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
  });

  test('creating element shows background collection tiles', async () => {
    await setInitialSettings(2);
    const collections = categoriesElement.shadowRoot!.querySelectorAll('.tile');
    assertEquals(2, collections.length);
    assertEquals(
        'collection_1', collections[0]!.querySelector('.label')!.textContent);
    assertEquals(
        'collection_2', collections[1]!.querySelector('.label')!.textContent);
  });

  test('clicking collection sends event', async () => {
    await setInitialSettings(1);

    const eventPromise = eventToPromise('collection-select', categoriesElement);
    const category = categoriesElement.shadowRoot!.querySelector('.tile')! as
        HTMLButtonElement;
    category.click();
    const event = (await eventPromise) as CustomEvent<BackgroundCollection>;
    assertTrue(!!event);
    assertEquals(event.detail.label, 'collection_1');
  });

  test('back button creates event', async () => {
    await setInitialSettings(0);
    const eventPromise = eventToPromise('back-click', categoriesElement);
    categoriesElement.$.backButton.click();
    const event = await eventPromise;
    assertTrue(!!event);
  });
});
