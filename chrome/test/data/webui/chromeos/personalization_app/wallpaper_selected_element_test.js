// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-selected component.  */

import {ActionName} from 'chrome://personalization/trusted/personalization_actions.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_reducers.js';
import {Paths} from 'chrome://personalization/trusted/personalization_router_element.js';
import {mockTimeoutForTesting, WallpaperSelected} from 'chrome://personalization/trusted/wallpaper_selected_element.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNotReached, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';
import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperSelectedTest() {
  /** @type {?HTMLElement} */
  let wallpaperSelectedElement = null;

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
    if (wallpaperSelectedElement) {
      wallpaperSelectedElement.remove();
    }
    wallpaperSelectedElement = null;
    await flushTasks();
  });

  test(
      'shows loading placeholder when there are in-flight requests',
      async () => {
        personalizationStore.data.loading = {
          ...personalizationStore.data.loading,
          selected: 1,
          setImage: 0,
        };
        wallpaperSelectedElement = initElement(WallpaperSelected.is);

        assertEquals(
            null, wallpaperSelectedElement.shadowRoot.querySelector('img'));

        assertEquals(
            null,
            wallpaperSelectedElement.shadowRoot.getElementById(
                'textContainer'));

        const placeholder = wallpaperSelectedElement.shadowRoot.getElementById(
            'imagePlaceholder');

        assertTrue(!!placeholder);

        // Loading placeholder should be hidden.
        personalizationStore.data.loading = {
          ...personalizationStore.data.loading,
          selected: 0,
          setImage: 0,
        };
        personalizationStore.data.currentSelected =
            wallpaperProvider.currentWallpaper;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        assertEquals('none', placeholder.style.display);

        // Sent a request to update user wallpaper. Loading placeholder should
        // come back.
        personalizationStore.data.loading = {
          ...personalizationStore.data.loading,
          selected: 0,
          setImage: 1,
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        assertEquals('', placeholder.style.display);
      });

  test('sets wallpaper image in store on first load', async () => {
    personalizationStore.expectAction(ActionName.SET_SELECTED_IMAGE);
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    const action =
        await personalizationStore.waitForAction(ActionName.SET_SELECTED_IMAGE);
    assertDeepEquals(wallpaperProvider.currentWallpaper, action.image);
  });

  test('shows wallpaper image and attribution when loaded', async () => {
    personalizationStore.data.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    const textContainerElements =
        wallpaperSelectedElement.shadowRoot.querySelectorAll(
            '#textContainer span');

    // First span tag is 'Currently Set' text.
    assertEquals('currentlySet', textContainerElements[0].id);
    assertEquals(
        wallpaperSelectedElement.i18n('currentlySet'),
        textContainerElements[0].textContent);

    // Following text elements are for the photo attribution text.
    const attributionLines = Array.from(textContainerElements).slice(1);

    assertEquals(
        wallpaperProvider.currentWallpaper.attribution.length,
        attributionLines.length);
    wallpaperProvider.currentWallpaper.attribution.forEach((line, i) => {
      assertEquals(line, attributionLines[i].innerText);
    });
  });

  test('shows unknown for empty attribution', async () => {
    personalizationStore.data.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const title =
        wallpaperSelectedElement.shadowRoot.getElementById('imageTitle');
    assertEquals(
        wallpaperSelectedElement.i18n('unknownImageAttribution'),
        title.textContent.trim());
  });

  test('removes high resolution suffix from image url', async () => {
    personalizationStore.data.currentSelected = {
      url: {url: 'https://images.googleusercontent.com/abc12=w456'},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        'chrome://image/?https://images.googleusercontent.com/abc12', img.src);
  });

  test('updates image when store is updated', async () => {
    personalizationStore.data.currentSelected =
        wallpaperProvider.currentWallpaper;
    personalizationStore.data.loading.selected = false;

    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    personalizationStore.data.currentSelected = {
      url: {url: 'https://testing'},
      attribution: ['New attribution'],
      assetId: BigInt(100),
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals('chrome://image/?https://testing', img.src);
  });

  test('shows placeholders when image fails to load', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    // Still loading.
    personalizationStore.data.loading.selected = true;
    personalizationStore.data.currentSelected = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    const placeholder =
        wallpaperSelectedElement.shadowRoot.getElementById('imagePlaceholder');
    assertTrue(!!placeholder);

    // Loading finished and still no current wallpaper.
    personalizationStore.data.loading.selected = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    // Dom-if will set display: none if the element is hidden. Make sure it is
    // not hidden.
    assertNotEquals('none', placeholder.style.display);
    assertEquals(
        null, wallpaperSelectedElement.shadowRoot.querySelector('img'));
  });

  test('sets selected wallpaper data in store on changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);

    wallpaperSelectedElement = initElement(WallpaperSelected.is);

    await wallpaperProvider.whenCalled('setWallpaperObserver');

    personalizationStore.expectAction(ActionName.SET_SELECTED_IMAGE);
    wallpaperProvider.wallpaperObserverRemote.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    const {image} =
        await personalizationStore.waitForAction(ActionName.SET_SELECTED_IMAGE);

    assertDeepEquals(wallpaperProvider.currentWallpaper, image);
  });

  test('shows image url with data scheme', async () => {
    personalizationStore.data.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals('data:image/png;base64,abc=', img.src);
  });

  test('shows daily refresh option on the collection view', async () => {
    personalizationStore.data.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.loading.selected = false;

    wallpaperSelectedElement =
        initElement(WallpaperSelected.is, {'path': Paths.CollectionImages});
    await waitAfterNextRender(wallpaperSelectedElement);

    const dailyRefresh =
        wallpaperSelectedElement.shadowRoot.getElementById('dailyRefresh');
    assertTrue(!!dailyRefresh);

    const refreshWallpaper =
        wallpaperSelectedElement.shadowRoot.getElementById('refreshWallpaper');
    assertTrue(refreshWallpaper.hidden);
  });

  test(
      'shows refresh button only on collection with daily refresh enabled',
      async () => {
        personalizationStore.data.currentSelected = {
          url: {url: 'data:image/png;base64,abc='},
          attribution: [],
          assetId: BigInt(100),
        };
        personalizationStore.data.loading.selected = false;
        const collection_id = wallpaperProvider.collections[0].id;
        personalizationStore.data.dailyRefresh = {
          collectionId: collection_id,
        };

        wallpaperSelectedElement = initElement(
            WallpaperSelected.is,
            {'path': Paths.CollectionImages, 'collectionId': collection_id});
        personalizationStore.notifyObservers();

        await waitAfterNextRender(wallpaperSelectedElement);

        const newRefreshWallpaper =
            wallpaperSelectedElement.shadowRoot.getElementById(
                'refreshWallpaper');
        assertFalse(newRefreshWallpaper.hidden);
      });

  test('sets current image to null after timeout', async () => {
    let timeoutCallback;
    mockTimeoutForTesting({
      setTimeout(callback, delay) {
        assertEquals(120 * 1000, delay);
        timeoutCallback = callback;
        return 1234;
      },
      clearTimeout(id) {
        assertNotReached('Should not clear timeout');
      },
    });

    wallpaperProvider.wallpaperObserverUpdateTimeout = 100;

    wallpaperSelectedElement =
        initElement(WallpaperSelected.is, {'path': Paths.CollectionImages});
    await waitAfterNextRender(wallpaperSelectedElement);

    personalizationStore.expectAction(ActionName.SET_SELECTED_IMAGE);

    timeoutCallback.call(wallpaperSelectedElement);

    const action =
        await personalizationStore.waitForAction(ActionName.SET_SELECTED_IMAGE);
    assertEquals(null, action.image);
  });

  test('cancels timeout after receiving first image', async () => {
    const timeoutId = 1234;
    const clearTimeoutPromise = new Promise(resolve => {
      mockTimeoutForTesting({
        setTimeout(callback, delay) {
          return timeoutId;
        },
        clearTimeout(id) {
          assertEquals(timeoutId, id);
          resolve();
        },
      });
    });

    wallpaperSelectedElement =
        initElement(WallpaperSelected.is, {'path': Paths.CollectionImages});
    await waitAfterNextRender(wallpaperSelectedElement);

    wallpaperProvider.wallpaperObserverRemote.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    await clearTimeoutPromise;
  });
}
