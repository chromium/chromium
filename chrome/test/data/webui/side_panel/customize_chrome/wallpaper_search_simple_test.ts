// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search_simple.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {WallpaperSearchSimpleElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search_simple.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from './test_support.js';

suite('WallpaperSearchSimpleTest', () => {
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let wallpaperSearchSimpleElement: WallpaperSearchSimpleElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    wallpaperSearchSimpleElement =
        document.createElement('customize-chrome-wallpaper-search-simple');
    document.body.appendChild(wallpaperSearchSimpleElement);
  });

  test('wallpaper search element added to side panel', async () => {
    assertTrue(document.body.contains(wallpaperSearchSimpleElement));
  });

  test('clicking search invokes backend', () => {
    wallpaperSearchSimpleElement.$.queryInput.value = 'foo';
    wallpaperSearchSimpleElement.$.submitButton.click();
    assertEquals(1, handler.getCallCount('searchWallpaper'));
    assertEquals('foo', handler.getArgs('searchWallpaper')[0]);
  });

  test('search failure shows error message', async () => {
    const failurePromise = Promise.resolve({success: false});
    handler.setResultFor('searchWallpaper', failurePromise);
    wallpaperSearchSimpleElement.$.submitButton.click();
    await failurePromise;
    assertTrue(wallpaperSearchSimpleElement.$.queryInput.invalid);
    assertEquals(
        'Error', wallpaperSearchSimpleElement.$.queryInput.errorMessage);
  });
});
