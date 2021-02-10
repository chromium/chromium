// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {WallpaperBrowserProxyImpl, routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestWallpaperBrowserProxy} from './test_wallpaper_browser_proxy.m.js';
// #import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('All', function() {
  /** @type {SettingsWallpaperSubpageElement} */
  let wallpaperSubpage = null;

  /** @type {?settings.TestWallpaperBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestWallpaperBrowserProxy();
    settings.WallpaperBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (wallpaperSubpage) {
      wallpaperSubpage.remove();
    }
    settings.Router.getInstance().resetRouteForTesting();
  });

  function initPage() {
    wallpaperSubpage = document.createElement('settings-wallpaper-page');
    document.body.appendChild(wallpaperSubpage);
    flush();
  }

  test(
      'fetches wallpaper collections and shows loading on startup',
      async () => {
        initPage();
        assertEquals(1, browserProxy.getCallCount('fetchWallpaperCollections'));

        const spinner = wallpaperSubpage.$$('paper-spinner-lite');
        assertTrue(!!spinner);
        assertTrue(spinner.active);
        assertFalse(spinner.hidden);

        const ironList = wallpaperSubpage.$$('iron-list');
        assertFalse(!!ironList);
      });

  test('shows wallpaper collections when loaded', async () => {
    initPage();

    const spinner = wallpaperSubpage.$$('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    await browserProxy.whenCalled('fetchWallpaperCollections');
    flush();

    assertFalse(spinner.active);
    assertTrue(spinner.hidden);

    const ironList = wallpaperSubpage.$$('iron-list');
    assertTrue(!!ironList);

    const elements = ironList.querySelectorAll('.wallpaper-collection-title');
    assertEquals(2, elements.length);

    assertDeepEquals(wallpaperSubpage.collections_, [
      {id: '0', name: 'zero'},
      {id: '1', name: 'one'},
    ]);
  });

  test('shows error when fails to load', async () => {
    browserProxy.setWallpaperCollections([]);
    initPage();

    const spinner = wallpaperSubpage.$$('paper-spinner-lite');
    assertTrue(spinner.active);

    // No error displayed while loading.
    const error = wallpaperSubpage.$$('#error');
    assertTrue(error.hidden);

    await browserProxy.whenCalled('fetchWallpaperCollections');
    Polymer.dom.flush();

    assertFalse(spinner.active);
    assertFalse(error.hidden);

    // No elements should be displayed if there is an error.
    assertFalse(!!wallpaperSubpage.$$('iron-list'));
  });
});
