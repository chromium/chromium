// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('WallpaperSearchTest', () => {
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let wallpaperSearchElement: WallpaperSearchElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    wallpaperSearchElement =
        document.createElement('customize-chrome-wallpaper-search');
    document.body.appendChild(wallpaperSearchElement);
  });

  test('wallpaper search element added to side panel', async () => {
    assertTrue(document.body.contains(wallpaperSearchElement));
  });

  test('clicking back button creates event', async () => {
    const eventPromise = eventToPromise('back-click', wallpaperSearchElement);
    wallpaperSearchElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });

  test('clicking search invokes backend', () => {
    wallpaperSearchElement.$.queryInput.value = 'foo';
    wallpaperSearchElement.$.submitButton.click();
    assertEquals(1, handler.getCallCount('getWallpaperSearchResults'));
    assertEquals('foo', handler.getArgs('getWallpaperSearchResults')[0]);
  });

  test('empty result shows error message', async () => {
    const emptyResultPromise = Promise.resolve({results: []});
    handler.setResultFor('getWallpaperSearchResults', emptyResultPromise);
    wallpaperSearchElement.$.submitButton.click();
    await emptyResultPromise;
    assertTrue(wallpaperSearchElement.$.queryInput.invalid);
    assertEquals('Error', wallpaperSearchElement.$.queryInput.errorMessage);
  });

  test('empty result shows no tiles', async () => {
    const emptyResultPromise = Promise.resolve({results: []});
    handler.setResultFor('getWallpaperSearchResults', emptyResultPromise);

    wallpaperSearchElement.$.submitButton.click();
    await emptyResultPromise;
    await waitAfterNextRender(wallpaperSearchElement);

    assertTrue(!wallpaperSearchElement.shadowRoot!.querySelector('.tile'));
  });

  test('shows mix of filled and empty containers', async () => {
    const resultPromise = Promise.resolve({results: ['123', '456']});
    handler.setResultFor('getWallpaperSearchResults', resultPromise);

    wallpaperSearchElement.$.submitButton.click();
    await resultPromise;
    await waitAfterNextRender(wallpaperSearchElement);

    // There should always be 6 tiles total. Since there are 2 images in the
    // response, there should be 2 result tiles and the remaining 4 should be
    // empty.
    assertEquals(
        wallpaperSearchElement.shadowRoot!.querySelectorAll('.tile').length, 6);
    assertEquals(
        wallpaperSearchElement.shadowRoot!.querySelectorAll('.tile.result')
            .length,
        2);
    assertEquals(
        wallpaperSearchElement.shadowRoot!.querySelectorAll('.tile.empty')
            .length,
        4);
  });
});
