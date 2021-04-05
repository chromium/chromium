// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/personalization_app.js';
import {WallpaperImages} from 'chrome://personalization/wallpaper_images_element.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';
import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';

const fetchImagesMethod = 'fetchImagesForCollection';

export function WallpaperImagesTest() {
  /** @type {?HTMLElement} */
  let wallpaperImagesElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  setup(() => {
    wallpaperProvider = baseSetup();
  });

  teardown(() => {
    if (wallpaperImagesElement) {
      wallpaperImagesElement.remove();
    }
  });

  test('fetches wallpaper images on collection-id change', async () => {
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {'collection-id': 'id_0'});

    let collectionId = await wallpaperProvider.whenCalled(fetchImagesMethod);
    assertEquals('id_0', collectionId);
    assertEquals(1, wallpaperProvider.getCallCount(fetchImagesMethod));
    await waitAfterNextRender(wallpaperImagesElement);

    wallpaperProvider.resetResolver(fetchImagesMethod);

    wallpaperImagesElement.setAttribute('collection-id', 'id_1');
    collectionId = await wallpaperProvider.whenCalled(fetchImagesMethod);
    assertEquals('id_1', collectionId);
    assertEquals(1, wallpaperProvider.getCallCount(fetchImagesMethod));
  });

  test('displays images for current collection id', async () => {
    wallpaperProvider.setImages([
      {url: {url: 'https://id_0-0/'}},
      {url: {url: 'https://id_0-1/'}},
    ]);

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {'collection-id': 'id_0'});

    let requestedId = await wallpaperProvider.whenCalled(fetchImagesMethod);
    assertEquals('id_0', requestedId);
    await waitAfterNextRender(wallpaperImagesElement);

    const ironList =
        wallpaperImagesElement.shadowRoot.querySelector('iron-list');
    assertTrue(!!ironList);

    const imageLinks = ironList.querySelectorAll('.wallpaper-image-link');

    assertEquals(2, imageLinks.length);
    assertEquals('https://id_0-0/', imageLinks[0].href);
    assertEquals('https://id_0-1/', imageLinks[1].href);

    wallpaperProvider.resetResolver(fetchImagesMethod);
    wallpaperProvider.setImages([
      {url: {url: 'https://id_1-0/'}},
      {url: {url: 'https://id_1-1/'}},
    ]);
    wallpaperImagesElement.setAttribute('collection-id', 'id_1');

    requestedId = await wallpaperProvider.whenCalled(fetchImagesMethod);
    assertEquals('id_1', requestedId);
    await waitAfterNextRender(wallpaperImagesElement);


    assertEquals(2, imageLinks.length);
    assertEquals('https://id_1-0/', imageLinks[0].href);
    assertEquals('https://id_1-1/', imageLinks[1].href);
  });

  test('displays error on loading failure', async () => {
    wallpaperProvider.setImagesToFail();

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {'collection-id': 'id_0'});

    const spinner =
        wallpaperImagesElement.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(spinner.active);

    const error = wallpaperImagesElement.shadowRoot.querySelector('#error');
    assertTrue(error.hidden);

    await wallpaperProvider.whenCalled(fetchImagesMethod);
    await waitAfterNextRender(wallpaperImagesElement);

    const ironList =
        wallpaperImagesElement.shadowRoot.querySelector('iron-list');
    assertFalse(!!ironList);

    assertFalse(spinner.active);
    assertFalse(error.hidden);
  });
}
