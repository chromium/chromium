// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/personalization_app.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import {WallpaperCollections} from 'chrome://personalization/wallpaper_collections_element.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';
import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';

export function WallpaperCollectionsTest() {
  /** @type {?HTMLElement} */
  let wallpaperCollectionsElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  setup(function() {
    wallpaperProvider = baseSetup();
  });

  teardown(function() {
    if (wallpaperCollectionsElement) {
      wallpaperCollectionsElement.remove();
    }
  });

  test(
      'fetches wallpaper collections and shows loading on startup',
      async () => {
        wallpaperCollectionsElement = initElement(WallpaperCollections.is);
        assertEquals(1, wallpaperProvider.getCallCount('fetchCollections'));

        const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
            'paper-spinner-lite');
        assertTrue(!!spinner);
        assertTrue(spinner.active);

        const ironList =
            wallpaperCollectionsElement.shadowRoot.querySelector('iron-list');
        assertFalse(!!ironList);
      });

  test('shows wallpaper collections when loaded', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    await wallpaperProvider.whenCalled('fetchCollections');
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(spinner.active);

    const ironList =
        wallpaperCollectionsElement.shadowRoot.querySelector('iron-list');
    assertTrue(!!ironList);

    const elements = ironList.querySelectorAll('.wallpaper-collection-title');
    assertEquals(2, elements.length);

    assertEquals('zero', elements[0].innerText);
    assertEquals('one', elements[1].innerText);
  });

  test('shows error when fails to load', async () => {
    wallpaperProvider.setCollectionsToFail();
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(spinner.active);

    // No error displayed while loading.
    const error =
        wallpaperCollectionsElement.shadowRoot.querySelector('#error');
    assertTrue(error.hidden);

    await wallpaperProvider.whenCalled('fetchCollections');
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(spinner.active);
    assertFalse(error.hidden);

    // No elements should be displayed if there is an error.
    assertFalse(
        !!wallpaperCollectionsElement.shadowRoot.querySelector('iron-list'));
  });

  test('sets href to images subpage', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);
    await wallpaperProvider.whenCalled('fetchCollections');
    await waitAfterNextRender(wallpaperCollectionsElement);

    const wallpaperCollections =
        wallpaperCollectionsElement.shadowRoot.querySelector('iron-list')
            .querySelectorAll('.wallpaper-collection-title');
    assertEquals(2, wallpaperCollections.length);

    assertEquals('A', wallpaperCollections[0].tagName);
    assertEquals(
        'chrome://personalization/collection?id=id_0',
        wallpaperCollections[0].href);
  });
}
