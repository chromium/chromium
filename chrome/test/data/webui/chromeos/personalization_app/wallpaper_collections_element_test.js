// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {emptyState} from 'chrome://personalization/trusted/personalization_reducers.js';
import {promisifySendCollectionsForTesting, WallpaperCollections} from 'chrome://personalization/trusted/wallpaper_collections_element.js';
import {assertDeepEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';
import {assertWindowObjectsEqual, baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperCollectionsTest() {
  /** @type {?HTMLElement} */
  let wallpaperCollectionsElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  setup(function() {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(wallpaperCollectionsElement);
    wallpaperCollectionsElement = null;
  });

  test('shows loading on startup', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    const iframe =
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe');
    assertTrue(iframe.hidden);
  });

  test('shows wallpaper collections when loaded', async () => {
    const sendCollectionsPromise = promisifySendCollectionsForTesting();
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    personalizationStore.data.loading = {collections: false};
    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    // Wait for |sendCollections| to be called.
    const [target, data] = await sendCollectionsPromise;
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(spinner.active);

    const iframe =
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe');
    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, target);
    assertDeepEquals(wallpaperProvider.collections, data);
  });

  test('shows error when fails to load', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(spinner.active);

    // No error displayed while loading.
    const error =
        wallpaperCollectionsElement.shadowRoot.querySelector('#error');
    assertTrue(error.hidden);

    personalizationStore.data.loading = {collections: false};
    personalizationStore.data.backdrop.collections = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(spinner.active);
    assertFalse(error.hidden);

    // Iframe should be hidden if there is an error.
    assertTrue(
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe').hidden);
  });

  test('loads backdrop data and saves to store', async () => {
    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);

    const sendCollectionsPromise = promisifySendCollectionsForTesting();
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const [_, data] = await sendCollectionsPromise;
    assertDeepEquals(wallpaperProvider.collections, data);

    assertDeepEquals(
        {
          collections: wallpaperProvider.collections,
          images: {
            'id_0': wallpaperProvider.images,
            'id_1': wallpaperProvider.images,
          },
        },
        personalizationStore.data.backdrop,
    );
    assertDeepEquals(
        {
          ...emptyState().loading,
          collections: false,
          images: {
            'id_0': false,
            'id_1': false,
          },
        },
        personalizationStore.data.loading,
    );
  });
}
