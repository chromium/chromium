// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {emptyState, FullscreenPreviewState, setFullscreenStateAction, setSelectedImageAction, SetSelectedImageAction, WallpaperActionName, WallpaperObserver} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {baseSetup} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperObserverTest', function() {
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
    // WallpaperObserver was initialized from ./wallpaper/index.js which causes
    // issues with this test. Force it to shutdown first to make the test work
    // properly.
    WallpaperObserver.shutdown();
    WallpaperObserver.initWallpaperObserverIfNeeded();
  });

  teardown(() => {
    WallpaperObserver.shutdown();
  });

  test('sets wallpaper image in store on first load', async () => {
    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);
    const action =
        await personalizationStore.waitForAction(
            WallpaperActionName.SET_SELECTED_IMAGE) as SetSelectedImageAction;
    assertDeepEquals(wallpaperProvider.currentWallpaper, action.image);
  });

  test('sets selected wallpaper data in store on changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);

    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);
    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    const {image} =
        await personalizationStore.waitForAction(
            WallpaperActionName.SET_SELECTED_IMAGE) as SetSelectedImageAction;

    assertDeepEquals(wallpaperProvider.currentWallpaper, image);
  });

  test('sets selected wallpaper if null', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);

    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);
    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(null);

    const {image} =
        await personalizationStore.waitForAction(
            WallpaperActionName.SET_SELECTED_IMAGE) as SetSelectedImageAction;
    assertEquals(null, image);
  });

  test('OnWallpaperChange updates fullscreen state from loading', async () => {
    personalizationStore.data.wallpaper.fullscreen =
        FullscreenPreviewState.LOADING;

    personalizationStore.expectAction(WallpaperActionName.SET_FULLSCREEN_STATE);

    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    assertDeepEquals(
        setFullscreenStateAction(FullscreenPreviewState.VISIBLE),
        await personalizationStore.waitForAction(
            WallpaperActionName.SET_FULLSCREEN_STATE),
        'full screen set to visible');

    personalizationStore.data.wallpaper.fullscreen = FullscreenPreviewState.OFF;
    personalizationStore.notifyObservers();

    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);

    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);


    assertDeepEquals(
        setSelectedImageAction(wallpaperProvider.currentWallpaper),
        await personalizationStore.waitForAction(
            WallpaperActionName.SET_SELECTED_IMAGE),
        `${WallpaperActionName.SET_SELECTED_IMAGE} action sent`);
  });
});
