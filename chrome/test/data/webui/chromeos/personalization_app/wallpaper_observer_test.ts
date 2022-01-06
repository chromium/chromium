// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {WallpaperActionName} from 'chrome://personalization/trusted/wallpaper/wallpaper_actions.js';
import {WallpaperObserver} from 'chrome://personalization/trusted/wallpaper/wallpaper_observer.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {baseSetup} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function WallpaperObserverTest() {
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
    WallpaperObserver.initWallpaperObserverIfNeeded();
  });

  teardown(() => {
    WallpaperObserver.shutdown();
  });

  test('sets wallpaper image in store on first load', async () => {
    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);
    const action = await personalizationStore.waitForAction(
        WallpaperActionName.SET_SELECTED_IMAGE);
    assertDeepEquals(wallpaperProvider.currentWallpaper, action.image);
  });

  test('sets selected wallpaper data in store on changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);

    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);
    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    const {image} = await personalizationStore.waitForAction(
        WallpaperActionName.SET_SELECTED_IMAGE);

    assertDeepEquals(wallpaperProvider.currentWallpaper, image);
  });

  test('skips updating OnWallpaperChange while in fullscreen', async () => {
    personalizationStore.data.wallpaper.fullscreen = true;

    personalizationStore.resetLastAction();

    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    assertEquals(null, personalizationStore.lastAction);

    personalizationStore.data.wallpaper.fullscreen = false;
    personalizationStore.notifyObservers();

    personalizationStore.expectAction(WallpaperActionName.SET_SELECTED_IMAGE);

    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    const action = await personalizationStore.waitForAction(
        WallpaperActionName.SET_SELECTED_IMAGE);

    assertDeepEquals(
        {
          name: WallpaperActionName.SET_SELECTED_IMAGE,
          image: wallpaperProvider.currentWallpaper,
        },
        action);
  });
}