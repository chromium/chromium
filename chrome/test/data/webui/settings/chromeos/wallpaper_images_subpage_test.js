// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {Router, routes, WallpaperBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestWallpaperBrowserProxy} from './test_wallpaper_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// clang-format on

const fetchImagesMethod = 'fetchImagesForCollection';

suite('All', () => {
  let wallpaperImagesSubpage = null;

  let browserProxy = null;

  let router = null;

  setup(() => {
    browserProxy = new TestWallpaperBrowserProxy();
    settings.WallpaperBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    router = settings.Router.getInstance();
  });

  teardown(() => {
    if (wallpaperImagesSubpage) {
      wallpaperImagesSubpage.remove();
    }
    router.resetRouteForTesting();
  });

  function initPage() {
    wallpaperImagesSubpage =
        document.createElement('settings-wallpaper-images-page');
    document.body.appendChild(wallpaperImagesSubpage);
    flush();
  }

  function navigateTo(collectionId, route = settings.routes.WALLPAPER_IMAGES) {
    const params = new URLSearchParams();
    params.append('collection', collectionId);
    router.navigateTo(route, params);
  }

  test('fetches wallpaper images on route change', async () => {
    initPage();
    navigateTo('id_0');

    let collectionId = await browserProxy.whenCalled(fetchImagesMethod);
    assertEquals('id_0', collectionId);
    assertEquals(1, browserProxy.getCallCount(fetchImagesMethod));
    flush();

    browserProxy.resetResolver(fetchImagesMethod);

    navigateTo('id_1');
    collectionId = await browserProxy.whenCalled(fetchImagesMethod);
    assertEquals('id_1', collectionId);
    assertEquals(1, browserProxy.getCallCount(fetchImagesMethod));
  });

  test('displays images for current collection id', async () => {
    browserProxy.setWallpaperImages([
      {url: 'https://id_0-0/'},
      {url: 'https://id_0-1/'},
    ]);
    initPage();
    navigateTo('id_0');

    await browserProxy.whenCalled(fetchImagesMethod);
    flush();

    const ironList = wallpaperImagesSubpage.$$('iron-list');
    assertTrue(!!ironList);

    const imageLinks = ironList.querySelectorAll('.wallpaper-image-link');

    assertEquals(2, imageLinks.length);
    assertEquals('https://id_0-0/', imageLinks[0].href);
    assertEquals('https://id_0-1/', imageLinks[1].href);

    browserProxy.setWallpaperImages([
      {url: 'https://id_1-0/'},
      {url: 'https://id_1-1/'},
    ]);
    navigateTo('id_1');

    await browserProxy.whenCalled(fetchImagesMethod);
    flush();

    assertEquals(2, imageLinks.length);
    assertEquals('https://id_1-0/', imageLinks[0].href);
    assertEquals('https://id_1-1/', imageLinks[1].href);
  });

  test('displays error on loading failure', async () => {
    browserProxy.setWallpaperImages([]);

    initPage();
    navigateTo('id_0');

    const spinner = wallpaperImagesSubpage.$$('paper-spinner-lite');
    assertTrue(spinner.active);

    const error = wallpaperImagesSubpage.$$('#error');
    assertTrue(error.hidden);

    await browserProxy.whenCalled(fetchImagesMethod);

    const ironList = wallpaperImagesSubpage.$$('iron-list');
    assertFalse(!!ironList);

    assertFalse(spinner.active);
    assertFalse(error.hidden);
  });
});
