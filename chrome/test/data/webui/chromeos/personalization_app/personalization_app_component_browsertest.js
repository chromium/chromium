// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://personalization. Tests individual
 * polymer components in isolation.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/public/cpp/style/dark_light_mode_controller.h"');
GEN('#include "ash/webui/personalization_app/test/personalization_app_mojom_banned_browsertest_fixture.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var PersonalizationAppComponentBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://personalization/';
  }

  get testGenPreamble() {
    // Force light mode in test to reduce randomness.
    return () => {
      GEN('ash::DarkLightModeController::Get()');
      GEN('->SetDarkModeEnabledForTest(false);');
    };
  }

  get typedefCppFixture() {
    return 'ash::personalization_app::' +
        'PersonalizationAppMojomBannedBrowserTestFixture';
  }
};

[['AmbientObserverTest', 'ambient_observer_test.js'],
 ['AmbientPreviewLargeElementTest', 'ambient_preview_large_element_test.js'],
 ['AmbientPreviewSmallElementTest', 'ambient_preview_small_element_test.js'],
 ['AmbientSubpageElementTest', 'ambient_subpage_element_test.js'],
 ['AvatarCameraElementTest', 'avatar_camera_element_test.js'],
 ['AvatarListElementTest', 'avatar_list_element_test.js'],
 ['DynamicColorElementTest', 'dynamic_color_element_test.js'],
 ['GooglePhotosAlbumsElementTest', 'google_photos_albums_element_test.js'],
 [
   'GooglePhotosCollectionElementTest',
   'google_photos_collection_element_test.js'
 ],
 [
   'GooglePhotosPhotosByAlbumIdElementTest',
   'google_photos_photos_by_album_id_element_test.js'
 ],
 ['GooglePhotosPhotosElementTest', 'google_photos_photos_element_test.js'],
 [
   'GooglePhotosSharedAlbumDialogElementTest',
   'google_photos_shared_album_dialog_element_test.js'
 ],
 [
   'GooglePhotosZeroStateElementTest',
   'google_photos_zero_state_element_test.js'
 ],
 ['KeyboardBacklightElementTest', 'keyboard_backlight_element_test.js'],
 ['LocalImagesElementTest', 'local_images_element_test.js'],
 [
   'PersonalizationBreadcrumbElementTest',
   'personalization_breadcrumb_element_test.js'
 ],
 ['PersonalizationMainElementTest', 'personalization_main_element_test.js'],
 ['PersonalizationRouterElementTest', 'personalization_router_element_test.js'],
 ['PersonalizationThemeElementTest', 'personalization_theme_element_test.js'],
 ['PersonalizationToastElementTest', 'personalization_toast_element_test.js'],
 ['UserPreviewElementTest', 'user_preview_element_test.js'],
 ['UserSubpageElementTest', 'user_subpage_element_test.js'],
 ['WallpaperCollectionsElementTest', 'wallpaper_collections_element_test.js'],
 ['WallpaperFullscreenElementTest', 'wallpaper_fullscreen_element_test.js'],
 ['WallpaperGridItemElementTest', 'wallpaper_grid_item_element_test.js'],
 ['WallpaperImagesElementTest', 'wallpaper_images_element_test.js'],
 ['WallpaperObserverTest', 'wallpaper_observer_test.js'],
 ['WallpaperPreviewElementTest', 'wallpaper_preview_element_test.js'],
 ['WallpaperSelectedElementTest', 'wallpaper_selected_element_test.js'],
 ['WallpaperSubpageElementTest', 'wallpaper_subpage_element_test.js'],
 ['ZoneCustomizationElementTest', 'zone_customization_element_test.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `PersonalizationAppComponent${testName}`;
  this[className] = class extends PersonalizationAppComponentBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://personalization/test_loader.html` +
          `?module=chromeos/personalization_app/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
