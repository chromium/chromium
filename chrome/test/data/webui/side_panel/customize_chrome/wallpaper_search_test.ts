// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    assertEquals(1, handler.getCallCount('searchWallpaper'));
    assertEquals('foo', handler.getArgs('searchWallpaper')[0]);
  });

  test('search failure shows error message', async () => {
    const failurePromise = Promise.resolve({success: false});
    handler.setResultFor('searchWallpaper', failurePromise);
    wallpaperSearchElement.$.submitButton.click();
    await failurePromise;
    assertTrue(wallpaperSearchElement.$.queryInput.invalid);
    assertEquals('Error', wallpaperSearchElement.$.queryInput.errorMessage);
  });
});
