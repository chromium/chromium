// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {AmbientSubpageTest} from './ambient_subpage_element_test.js';
import {AvatarCameraTest} from './avatar_camera_element_test.js';
import {AvatarListTest} from './avatar_list_element_test.js';
import {GooglePhotosAlbumsTest} from './google_photos_albums_element_test.js';
import {GooglePhotosCollectionTest} from './google_photos_collection_element_test.js';
import {GooglePhotosPhotosByAlbumIdTest} from './google_photos_photos_by_album_id_element_test.js';
import {GooglePhotosPhotosTest} from './google_photos_photos_element_test.js';
import {LocalImagesTest} from './local_images_element_test.js';
import {PersonalizationBreadcrumbTest} from './personalization_breadcrumb_element_test.js';
import {PersonalizationMainTest} from './personalization_main_element_test.js';
import {PersonalizationRouterTest} from './personalization_router_element_test.js';
import {PersonalizationThemeTest} from './personalization_theme_element_test.js';
import {PersonalizationToastTest} from './personalization_toast_element_test.js';
import {UserPreviewTest} from './user_preview_element_test.js';
import {UserSubpageTest} from './user_subpage_element_test.js';
import {WallpaperCollectionsTest} from './wallpaper_collections_element_test.js';
import {WallpaperFullscreenTest} from './wallpaper_fullscreen_element_test.js';
import {WallpaperGridItemTest} from './wallpaper_grid_item_element_test.js';
import {WallpaperImagesTest} from './wallpaper_images_element_test.js';
import {WallpaperObserverTest} from './wallpaper_observer_test.js';
import {WallpaperPreviewTest} from './wallpaper_preview_element_test.js';
import {WallpaperSelectedTest} from './wallpaper_selected_element_test.js';

// Mute console.warn during tests. Several tests intentionally hit asserts to
// verify errors are thrown, and fill test logs with misleading stacktraces.
window.console.warn = () => {};

const testCases = [
  AmbientSubpageTest,
  AvatarCameraTest,
  AvatarListTest,
  GooglePhotosAlbumsTest,
  GooglePhotosCollectionTest,
  GooglePhotosPhotosByAlbumIdTest,
  GooglePhotosPhotosTest,
  LocalImagesTest,
  PersonalizationBreadcrumbTest,
  PersonalizationMainTest,
  PersonalizationRouterTest,
  PersonalizationThemeTest,
  PersonalizationToastTest,
  UserPreviewTest,
  UserSubpageTest,
  WallpaperCollectionsTest,
  WallpaperFullscreenTest,
  WallpaperGridItemTest,
  WallpaperImagesTest,
  WallpaperObserverTest,
  WallpaperPreviewTest,
  WallpaperSelectedTest,
];

for (const testCase of testCases) {
  suite(testCase.name, testCase);
}
