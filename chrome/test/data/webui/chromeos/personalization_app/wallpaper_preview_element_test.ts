// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-preview component.  */

import 'chrome://personalization/strings.m.js';

import {WallpaperPreviewElement, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertNotEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperPreviewElementTest', function() {
  let wallpaperPreviewElement: WallpaperPreviewElement|null;
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
          selected: {attribution: true, image: true},
          setImage: 0,
        };
        wallpaperPreviewElement = initElement(WallpaperPreviewElement);

        assertEquals(
            null, wallpaperPreviewElement.shadowRoot!.querySelector('img'));

        const placeholder = wallpaperPreviewElement.shadowRoot!.getElementById(
            'imagePlaceholder');

        assertTrue(!!placeholder);

        // Loading placeholder should be hidden.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: {attribution: false, image: false},
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
          selected: {attribution: false, image: false},
          setImage: 1,
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperPreviewElement);

        assertEquals('', placeholder.style.display);
      });

  test('shows wallpaper image when loaded', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperPreviewElement = initElement(WallpaperPreviewElement);
    await waitAfterNextRender(wallpaperPreviewElement);

    const img = wallpaperPreviewElement.shadowRoot!.querySelector('img');
    assertStringContains(
        img!.src,
        `chrome://personalization/wallpaper.jpg?key=${
            wallpaperProvider.currentWallpaper.key}`);
  });

  test('shows placeholders when image fails to load', async () => {
    wallpaperPreviewElement = initElement(WallpaperPreviewElement);
    await waitAfterNextRender(wallpaperPreviewElement);

    // Still loading.
    personalizationStore.data.wallpaper.loading.selected.image = true;
    personalizationStore.data.wallpaper.loading.selected.attribution = true;
    personalizationStore.data.wallpaper.currentSelected = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperPreviewElement);

    const placeholder =
        wallpaperPreviewElement.shadowRoot!.getElementById('imagePlaceholder');
    assertTrue(!!placeholder);

    // Loading finished and still no current wallpaper.
    personalizationStore.data.wallpaper.loading.selected.image = false;
    personalizationStore.data.wallpaper.loading.selected.attribution = false;
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

    wallpaperPreviewElement = initElement(WallpaperPreviewElement);
    await waitAfterNextRender(wallpaperPreviewElement);

    function getManagedIcon(): HTMLElement|null {
      return wallpaperPreviewElement!.shadowRoot!.querySelector(
          `iron-icon[icon^='personalization:managed']`);
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
