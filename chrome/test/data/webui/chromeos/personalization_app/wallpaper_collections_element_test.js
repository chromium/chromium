// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {untrustedOrigin} from 'chrome://personalization/common/constants.js';
import {selectCollection} from 'chrome://personalization/common/iframe_api.js';
import {promisifySendCollectionsForTesting, WallpaperCollections} from 'chrome://personalization/trusted/wallpaper_collections_element.js';
import {assertDeepEquals, assertEquals, assertFalse, assertThrows, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';
import {assertWindowObjectsEqual, baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';

export function WallpaperCollectionsTest() {
  /** @type {?HTMLElement} */
  let wallpaperCollectionsElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  setup(function() {
    wallpaperProvider = baseSetup();
  });

  teardown(async () => {
    await teardownElement(wallpaperCollectionsElement);
    wallpaperCollectionsElement = null;
  });

  test(
      'fetches wallpaper collections and shows loading on startup',
      async () => {
        wallpaperCollectionsElement =
            initElement(WallpaperCollections.is, {active: true});
        await wallpaperProvider.whenCalled('fetchCollections');
        assertEquals(1, wallpaperProvider.getCallCount('fetchCollections'));

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
    wallpaperCollectionsElement =
        initElement(WallpaperCollections.is, {active: true});

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    // Wait for collections to be fetched.
    await wallpaperProvider.whenCalled('fetchCollections');
    // Wait for iframe to load and |sendCollections| to be called.
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
    wallpaperProvider.setCollectionsToFail();
    wallpaperCollectionsElement =
        initElement(WallpaperCollections.is, {active: true});

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

    // Iframe should be hidden if there is an error.
    assertTrue(
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe').hidden);
  });

  test(
      'calls selectCollection callback when SelectCollectionEvent is received',
      async () => {
        const sendCollectionsPromise = promisifySendCollectionsForTesting();
        wallpaperCollectionsElement =
            initElement(WallpaperCollections.is, {active: true});
        const selectCollectionPromise = new Promise((resolve) => {
          wallpaperCollectionsElement.selectCollection = resolve;
          const original = wallpaperCollectionsElement.onCollectionSelected_;
          function patched(event) {
            // Rewrite event to make it look as if coming from untrusted origin.
            original.call(
                wallpaperCollectionsElement,
                {data: event.data, origin: untrustedOrigin});
          }
          window.removeEventListener('message', original);
          window.addEventListener('message', patched, {once: true});
        });
        await sendCollectionsPromise;
        await waitAfterNextRender(wallpaperCollectionsElement);

        selectCollection(window, 'id_0');
        const id = await selectCollectionPromise;
        assertEquals('id_0', id);
      });

  test(
      'throws error when invalid SelectCollectionEvent is received',
      async () => {
        const sendCollectionsPromise = promisifySendCollectionsForTesting();
        wallpaperCollectionsElement =
            initElement(WallpaperCollections.is, {active: true});
        const original = wallpaperCollectionsElement.onCollectionSelected_;
        const selectCollectionPromise = new Promise((resolve) => {
          function patched(event) {
            // Rewrite event to make it look as if coming from untrusted origin.
            assertThrows(
                () => original.call(
                    wallpaperCollectionsElement,
                    {data: event.data, origin: untrustedOrigin}),
                'Assertion failed: No valid selection found in choices');
            resolve();
          }
          window.removeEventListener('message', original);
          window.addEventListener('message', patched, {once: true});
        });

        await sendCollectionsPromise;
        await waitAfterNextRender(wallpaperCollectionsElement);

        selectCollection(window, 'does_not_exist');
        // Wait for the message handler |patched| to run.
        await selectCollectionPromise;
      });
}
