// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocalImages} from 'chrome://personalization/trusted/local_images_element.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';
import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function LocalImagesTest() {
  /** @type {?HTMLElement} */
  let localImagesElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(localImagesElement);
    localImagesElement = null;
    await flushTasks();
  });

  test(
      'displays loading spinner while local image list is loading',
      async () => {
        personalizationStore.data.loading.local = {images: true, data: {}};

        localImagesElement = initElement(LocalImages.is, {hidden: false});

        const spinner =
            localImagesElement.shadowRoot.querySelector('paper-spinner-lite');
        assertTrue(spinner.active);

        personalizationStore.data.loading.local = {images: false, data: {}};
        personalizationStore.notifyObservers();

        await waitAfterNextRender(localImagesElement);

        assertFalse(spinner.active);
      });

  test(
      'displays images for current local images that have successfully loaded',
      async () => {
        personalizationStore.data.local = {
          images: wallpaperProvider.localImages,
          data: wallpaperProvider.localImageData,
        };
        personalizationStore.data.loading.local = {images: false, data: {}};

        localImagesElement = initElement(LocalImages.is, {hidden: false});

        const ironList =
            localImagesElement.shadowRoot.querySelector('iron-list');
        assertTrue(!!ironList);

        // Both items are sent. No images are rendered yet because they are not
        // done loading thumbnails.
        assertEquals(2, ironList.items.length);
        assertEquals(0, ironList.shadowRoot.querySelectorAll('img').length);

        // Set loading finished for first thumbnail.
        personalizationStore.data.loading.local.data = {'100,10': false};
        personalizationStore.notifyObservers();
        await waitAfterNextRender(localImagesElement);
        assertEquals(2, ironList.items.length);
        let imgTags = localImagesElement.shadowRoot.querySelectorAll('img');
        assertEquals(1, imgTags.length);
        assertEquals('data://localimage0data', imgTags[0].src);

        // Set loading failed for second thumbnail.
        personalizationStore.data.loading.local.data = {
          '100,10': false,
          '200,20': false
        };
        personalizationStore.data.local.data = {
          '100,10': 'data://localimage0data',
          '200,20': null
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(localImagesElement);
        // Still only first thumbnail displayed.
        imgTags = localImagesElement.shadowRoot.querySelectorAll('img');
        assertEquals(1, imgTags.length);
        assertEquals('data://localimage0data', imgTags[0].src);
      });

  test('displays error on loading failure', async () => {
    personalizationStore.data.local = {images: null, data: {}};
    personalizationStore.data.loading.local = {images: false, data: {}};

    localImagesElement = initElement(LocalImages.is, {hidden: false});

    // Spinner is not active because loading is finished.
    const spinner =
        localImagesElement.shadowRoot.querySelector('paper-spinner-lite');
    assertFalse(spinner.active);

    // Error should be visible.
    const error = localImagesElement.shadowRoot.querySelector('#error');
    assertFalse(error.hidden);

    // No iron-list displayed.
    const ironList = localImagesElement.shadowRoot.querySelector('iron-list');
    assertFalse(!!ironList);
  });
}
