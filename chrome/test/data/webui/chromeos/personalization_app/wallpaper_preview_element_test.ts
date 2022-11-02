// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-preview component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {WallpaperPreview, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperPreviewTest', function() {
  let wallpaperPreviewElement: WallpaperPreview|null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (wallpaperPreviewElement) {
      wallpaperPreviewElement.remove();
    }
    wallpaperPreviewElement = null;
    await flushTasks();
  });

  test(
      'shows loading placeholder when there are in-flight requests',
      async () => {
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 1,
          setImage: 0,
        };
        wallpaperPreviewElement = initElement(WallpaperPreview);

        assertEquals(
            null, wallpaperPreviewElement.shadowRoot!.querySelector('img'));

        const placeholder = wallpaperPreviewElement.shadowRoot!.getElementById(
            'imagePlaceholder');

        assertTrue(!!placeholder);

        // Loading placeholder should be hidden.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 0,
          setImage: 0,
        };
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperPreviewElement);

        assertEquals('none', placeholder.style.display);

        // Sent a request to update user wallpaper. Loading placeholder should
        // come back.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 0,
          setImage: 1,
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperPreviewElement);

        assertEquals('', placeholder.style.display);
      });

  test('shows wallpaper image when loaded', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperPreviewElement = initElement(WallpaperPreview);
    await waitAfterNextRender(wallpaperPreviewElement);

    const img = wallpaperPreviewElement.shadowRoot!.querySelector('img');
    assertEquals(
        `chrome://personalization/wallpaper.jpg?key=${
            wallpaperProvider.currentWallpaper.key}`,
        img!.src,
        'current wallpaper key is appended to url as query parameter');
  });

  test('shows placeholders when image fails to load', async () => {
    wallpaperPreviewElement = initElement(WallpaperPreview);
    await waitAfterNextRender(wallpaperPreviewElement);

    // Still loading.
    personalizationStore.data.wallpaper.loading.selected = true;
    personalizationStore.data.wallpaper.currentSelected = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperPreviewElement);

    const placeholder =
        wallpaperPreviewElement.shadowRoot!.getElementById('imagePlaceholder');
    assertTrue(!!placeholder);

    // Loading finished and still no current wallpaper.
    personalizationStore.data.wallpaper.loading.selected = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperPreviewElement);

    // Dom-if will set display: none if the element is hidden. Make sure it is
    // not hidden.
    assertNotEquals('none', placeholder.style.display);
    assertEquals(
        null, wallpaperPreviewElement.shadowRoot!.querySelector('img'));
  });

  test('shows managed icon when wallpaper is kPolicy', async () => {
    // Start with non-managed wallpaper.
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperPreviewElement = initElement(WallpaperPreview);
    await waitAfterNextRender(wallpaperPreviewElement);

    function getManagedIcon(): HTMLElement|null {
      return wallpaperPreviewElement!.shadowRoot!.querySelector(
          `iron-icon[icon='personalization:managed']`);
    }

    assertEquals(null, getManagedIcon(), 'no managed icon visible');

    personalizationStore.data.wallpaper.currentSelected = {
      ...personalizationStore.data.wallpaper.currentSelected,
      type: WallpaperType.kPolicy,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperPreviewElement);

    assertTrue(!!getManagedIcon(), 'managed icon is shown');
  });
});
