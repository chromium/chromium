// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/favicon_size.h"

namespace web_app {

namespace {

using IconSizeAndPurpose = AppIconManager::IconSizeAndPurpose;

}  // namespace

class WebAppIconManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());

    auto file_utils = std::make_unique<TestFileUtils>();
    file_utils_ = file_utils.get();

    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), registrar(),
                                                        std::move(file_utils));

    controller().Init();
  }

 protected:
  void WriteIcons(const AppId& app_id,
                  const std::vector<IconPurpose>& purposes,
                  const std::vector<int>& sizes_px,
                  const std::vector<SkColor>& colors) {
    DCHECK_EQ(sizes_px.size(), colors.size());
    DCHECK(!purposes.empty());

    IconBitmaps icon_bitmaps;
    for (size_t i = 0; i < sizes_px.size(); ++i) {
      std::string icon_name = base::StringPrintf("app-%d.ico", sizes_px[i]);
      if (base::Contains(purposes, IconPurpose::ANY)) {
        AddGeneratedIcon(&icon_bitmaps.any, sizes_px[i], colors[i]);
      }
      if (base::Contains(purposes, IconPurpose::MASKABLE)) {
        AddGeneratedIcon(&icon_bitmaps.maskable, sizes_px[i], colors[i]);
      }
      if (base::Contains(purposes, IconPurpose::MONOCHROME))
        // TODO (crbug.com/1114638): Monochrome icon support.
        NOTREACHED();
    }

    base::RunLoop run_loop;
    icon_manager_->WriteData(app_id, std::move(icon_bitmaps),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  void WriteShortcutsMenuIcons(const AppId& app_id,
                               const std::vector<int>& sizes_any,
                               const std::vector<SkColor>& colors_any,
                               const std::vector<int>& sizes_maskable,
                               const std::vector<SkColor>& colors_maskable) {
    DCHECK_EQ(sizes_any.size(), sizes_maskable.size());
    DCHECK_EQ(sizes_any.size(), colors_any.size());
    DCHECK_EQ(sizes_maskable.size(), colors_maskable.size());
    ShortcutsMenuIconBitmaps shortcuts_menu_icons;
    for (size_t i = 0; i < sizes_any.size(); i++) {
      IconBitmaps shortcuts_menu_icon_map;
      shortcuts_menu_icon_map.any.emplace(
          sizes_any[i], CreateSquareIcon(sizes_any[i], colors_any[i]));
      shortcuts_menu_icon_map.maskable.emplace(
          sizes_maskable[i],
          CreateSquareIcon(sizes_maskable[i], colors_maskable[i]));
      shortcuts_menu_icons.push_back(std::move(shortcuts_menu_icon_map));
    }
    base::RunLoop run_loop;
    icon_manager_->WriteShortcutsMenuIconsData(
        app_id, std::move(shortcuts_menu_icons),
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  ShortcutsMenuIconBitmaps ReadAllShortcutsMenuIcons(const AppId& app_id) {
    ShortcutsMenuIconBitmaps result;
    base::RunLoop run_loop;
    icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindLambdaForTesting(
                    [&](ShortcutsMenuIconBitmaps shortcuts_menu_icons_map) {
                      result = std::move(shortcuts_menu_icons_map);
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return result;
  }

  std::vector<IconSizes> CreateDownloadedShortcutsMenuIconsSizes(
      std::vector<SquareSizePx> sizes_px) {
    std::vector<IconSizes> downloaded_shortcuts_menu_icons_sizes;
    for (const auto& size : sizes_px) {
      IconSizes icon_sizes;
      {
        std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
        shortcuts_menu_icon_sizes_any.push_back(size);
        icon_sizes.SetSizesForPurpose(IconPurpose::ANY,
                                      std::move(shortcuts_menu_icon_sizes_any));
      }
      downloaded_shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
    }
    return downloaded_shortcuts_menu_icons_sizes;
  }

  std::vector<IconSizes> CreateDownloadedShortcutsMenuIconsSizes(
      std::vector<SquareSizePx> sizes_any,
      std::vector<SquareSizePx> sizes_maskable) {
    DCHECK_EQ(sizes_any.size(), sizes_maskable.size());

    std::vector<IconSizes> downloaded_shortcuts_menu_icons_sizes;
    for (size_t index = 0; index < sizes_any.size(); ++index) {
      IconSizes icon_sizes;
      {
        std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
        shortcuts_menu_icon_sizes_any.push_back(sizes_any[index]);
        icon_sizes.SetSizesForPurpose(IconPurpose::ANY,
                                      std::move(shortcuts_menu_icon_sizes_any));
      }
      {
        std::vector<SquareSizePx> shortcuts_menu_icon_sizes_maskable;
        shortcuts_menu_icon_sizes_maskable.push_back(sizes_maskable[index]);
        icon_sizes.SetSizesForPurpose(
            IconPurpose::MASKABLE,
            std::move(shortcuts_menu_icon_sizes_maskable));
      }
      downloaded_shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
    }
    return downloaded_shortcuts_menu_icons_sizes;
  }

  struct PurposeAndBitmap {
    IconPurpose purpose;
    SkBitmap bitmap;
  };
  PurposeAndBitmap ReadSmallestIcon(const AppId& app_id,
                                    const std::vector<IconPurpose>& purposes,
                                    SquareSizePx min_icon_size) {
    PurposeAndBitmap result;
    base::RunLoop run_loop;
    icon_manager().ReadSmallestIcon(
        app_id, purposes, min_icon_size,
        base::BindLambdaForTesting(
            [&](IconPurpose purpose, const SkBitmap& bitmap) {
              result.purpose = purpose;
              result.bitmap = bitmap;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  SkBitmap ReadSmallestIconAny(const AppId& app_id,
                               SquareSizePx min_icon_size) {
    SkBitmap result;
    base::RunLoop run_loop;
    icon_manager().ReadSmallestIconAny(
        app_id, min_icon_size,
        base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
          result = bitmap;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  struct PurposeAndData {
    IconPurpose purpose;
    std::vector<uint8_t> data;
  };
  PurposeAndData ReadSmallestCompressedIcon(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      int min_size_in_px) {
    EXPECT_TRUE(
        icon_manager().HasSmallestIcon(app_id, purposes, min_size_in_px));

    PurposeAndData result;

    base::RunLoop run_loop;
    icon_manager().ReadSmallestCompressedIcon(
        app_id, purposes, min_size_in_px,
        base::BindLambdaForTesting(
            [&](IconPurpose purpose, std::vector<uint8_t> data) {
              result.purpose = purpose;
              result.data = std::move(data);
              run_loop.Quit();
            }));

    run_loop.Run();
    return result;
  }

  SkColor ReadIconAndResize(const AppId& app_id,
                            IconPurpose purpose,
                            int desired_icon_size) {
    base::RunLoop run_loop;
    SkColor icon_color = SK_ColorBLACK;

    icon_manager().ReadIconAndResize(
        app_id, purpose, desired_icon_size,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(1u, icon_bitmaps.size());
              SkBitmap bitmap = icon_bitmaps[desired_icon_size];
              EXPECT_FALSE(bitmap.empty());
              EXPECT_EQ(desired_icon_size, bitmap.width());
              EXPECT_EQ(desired_icon_size, bitmap.height());
              icon_color = bitmap.getColor(0, 0);

              run_loop.Quit();
            }));

    run_loop.Run();
    return icon_color;
  }

  SkColor ReadIconAndResize(const AppId& app_id, int desired_icon_size) {
    return ReadIconAndResize(app_id, IconPurpose::ANY, desired_icon_size);
  }

  std::unique_ptr<WebApp> CreateWebApp() {
    const GURL app_url = GURL("https://example.com/path");
    const AppId app_id = GenerateAppIdFromURL(app_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(Source::kSync);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    web_app->SetName("Name");
    web_app->SetStartUrl(app_url);

    return web_app;
  }

  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;

  // Owned by icon_manager_:
  TestFileUtils* file_utils_ = nullptr;
};

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_AnyOnly) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k256].empty());
              EXPECT_EQ(SK_ColorGREEN,
                        icon_bitmaps[icon_size::k256].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k512].empty());
              EXPECT_EQ(SK_ColorYELLOW,
                        icon_bitmaps[icon_size::k512].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
  EXPECT_FALSE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));
}

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_MaskableOnly) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::MASKABLE}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));
  EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::MASKABLE, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k256].empty());
              EXPECT_EQ(SK_ColorGREEN,
                        icon_bitmaps[icon_size::k256].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k512].empty());
              EXPECT_EQ(SK_ColorYELLOW,
                        icon_bitmaps[icon_size::k512].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_AnyAndMaskable) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k256].empty());
              EXPECT_EQ(SK_ColorGREEN,
                        icon_bitmaps[icon_size::k256].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k512].empty());
              EXPECT_EQ(SK_ColorYELLOW,
                        icon_bitmaps[icon_size::k512].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
  EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::MASKABLE, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k256].empty());
              EXPECT_EQ(SK_ColorGREEN,
                        icon_bitmaps[icon_size::k256].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k512].empty());
              EXPECT_EQ(SK_ColorYELLOW,
                        icon_bitmaps[icon_size::k512].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, OverwriteIcons) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  // Write initial red icons to be overwritten.
  {
    std::vector<int> sizes_px{icon_size::k32, icon_size::k64, icon_size::k48};
    const std::vector<SkColor> colors{SK_ColorRED, SK_ColorRED, SK_ColorRED};
    WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
               colors);

    web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
    web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, std::move(sizes_px));
  }

  controller().RegisterApp(std::move(web_app));

  // k64 and k48 sizes to be overwritten. Skip k32 size and add new k96 size.
  const std::vector<int> overwritten_sizes_px{icon_size::k48, icon_size::k64,
                                              icon_size::k96};
  {
    IconBitmaps icon_bitmaps;
    for (int size_px : overwritten_sizes_px) {
      icon_bitmaps.any[size_px] = CreateSquareIcon(size_px, SK_ColorGREEN);
      icon_bitmaps.maskable[size_px] = CreateSquareIcon(size_px, SK_ColorBLUE);
    }

    base::RunLoop run_loop;

    // Overwrite red icons with green and blue ones.
    icon_manager().WriteData(app_id, std::move(icon_bitmaps),
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));

    run_loop.Run();

    ScopedRegistryUpdate update(&controller().sync_bridge());
    update->UpdateApp(app_id)->SetDownloadedIconSizes(IconPurpose::ANY,
                                                      overwritten_sizes_px);
    update->UpdateApp(app_id)->SetDownloadedIconSizes(IconPurpose::MASKABLE,
                                                      overwritten_sizes_px);
  }

  // Check that all IconPurpose::ANY icons are now green. Check that all red
  // icons were deleted on disk (including the k32 size).
  {
    base::FilePath icons_dir = GetAppIconsAnyDir(profile(), app_id);
    std::vector<int> sizes_on_disk_px;

    base::FileEnumerator enumerator_any(icons_dir, true,
                                        base::FileEnumerator::FILES);
    for (base::FilePath path = enumerator_any.Next(); !path.empty();
         path = enumerator_any.Next()) {
      EXPECT_TRUE(path.MatchesExtension(FILE_PATH_LITERAL(".png")));

      SkBitmap bitmap;
      EXPECT_TRUE(ReadBitmap(&file_utils(), path, &bitmap));
      EXPECT_FALSE(bitmap.empty());
      EXPECT_EQ(bitmap.width(), bitmap.height());
      EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));

      sizes_on_disk_px.push_back(bitmap.width());
    }

    std::sort(sizes_on_disk_px.begin(), sizes_on_disk_px.end());
    EXPECT_EQ(overwritten_sizes_px, sizes_on_disk_px);
  }

  // Check that all IconPurpose::Maskable icons are now blue. Check that all red
  // icons were deleted on disk (including the k32 size).
  {
    base::FilePath icons_dir = GetAppIconsMaskableDir(profile(), app_id);
    std::vector<int> sizes_on_disk_px;

    base::FileEnumerator enumerator_maskable(icons_dir, true,
                                             base::FileEnumerator::FILES);
    for (base::FilePath path = enumerator_maskable.Next(); !path.empty();
         path = enumerator_maskable.Next()) {
      EXPECT_TRUE(path.MatchesExtension(FILE_PATH_LITERAL(".png")));

      SkBitmap bitmap;
      EXPECT_TRUE(ReadBitmap(&file_utils(), path, &bitmap));
      EXPECT_FALSE(bitmap.empty());
      EXPECT_EQ(bitmap.width(), bitmap.height());
      EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));

      sizes_on_disk_px.push_back(bitmap.width());
    }

    std::sort(sizes_on_disk_px.begin(), sizes_on_disk_px.end());
    EXPECT_EQ(overwritten_sizes_px, sizes_on_disk_px);
  }
}

TEST_F(WebAppIconManagerTest, ReadAllIcons_AnyOnly) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));
  {
    base::RunLoop run_loop;

    icon_manager().ReadAllIcons(
        app_id, base::BindLambdaForTesting([&](IconBitmaps icons_map) {
          EXPECT_FALSE(icons_map.empty());
          EXPECT_EQ(2u, icons_map.any.size());
          EXPECT_EQ(colors[0], icons_map.any[sizes_px[0]].getColor(0, 0));
          EXPECT_EQ(colors[1], icons_map.any[sizes_px[1]].getColor(0, 0));
          EXPECT_EQ(0u, icons_map.maskable.size());
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, ReadAllIcons_AnyAndMaskable) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  controller().RegisterApp(std::move(web_app));
  {
    base::RunLoop run_loop;

    icon_manager().ReadAllIcons(
        app_id, base::BindLambdaForTesting([&](IconBitmaps icons_map) {
          EXPECT_FALSE(icons_map.empty());
          EXPECT_EQ(2u, icons_map.any.size());
          EXPECT_EQ(colors[0], icons_map.any[sizes_px[0]].getColor(0, 0));
          EXPECT_EQ(colors[1], icons_map.any[sizes_px[1]].getColor(0, 0));
          EXPECT_EQ(2u, icons_map.maskable.size());
          EXPECT_EQ(colors[0], icons_map.maskable[sizes_px[0]].getColor(0, 0));
          EXPECT_EQ(colors[1], icons_map.maskable[sizes_px[1]].getColor(0, 0));
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, ReadShortcutsMenuIconsFailed) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<SquareSizePx> sizes_px{icon_size::k96, icon_size::k256};

  // Set shortcuts menu icons meta-info but don't write bitmaps to disk.
  web_app->SetDownloadedShortcutsMenuIconsSizes(
      CreateDownloadedShortcutsMenuIconsSizes(sizes_px));

  controller().RegisterApp(std::move(web_app));

  // Request shortcuts menu icons which don't exist on disk.
  ShortcutsMenuIconBitmaps shortcuts_menu_icons_map =
      ReadAllShortcutsMenuIcons(app_id);
  EXPECT_EQ(sizes_px.size(), shortcuts_menu_icons_map.size());
  for (const auto& icon_map : shortcuts_menu_icons_map) {
    EXPECT_TRUE(icon_map.empty());
  }
}

TEST_F(WebAppIconManagerTest, WriteAndReadAllShortcutsMenuIcons) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_any = {icon_size::k64, icon_size::k128,
                                      icon_size::k256};
  const std::vector<SkColor> colors_any = {SK_ColorRED, SK_ColorWHITE,
                                           SK_ColorBLUE};
  const std::vector<int> sizes_maskable = {icon_size::k64, icon_size::k96,
                                           icon_size::k128};
  const std::vector<SkColor> colors_maskable = {SK_ColorCYAN, SK_ColorMAGENTA,
                                                SK_ColorYELLOW};

  WriteShortcutsMenuIcons(app_id, sizes_any, colors_any, sizes_maskable,
                          colors_maskable);

  web_app->SetDownloadedShortcutsMenuIconsSizes(
      CreateDownloadedShortcutsMenuIconsSizes(sizes_any, sizes_maskable));

  controller().RegisterApp(std::move(web_app));

  ShortcutsMenuIconBitmaps shortcuts_menu_icons_map =
      ReadAllShortcutsMenuIcons(app_id);
  EXPECT_EQ(3u, shortcuts_menu_icons_map.size());
  EXPECT_EQ(sizes_any[0], shortcuts_menu_icons_map[0].any.begin()->first);
  EXPECT_EQ(colors_any[0],
            shortcuts_menu_icons_map[0].any.begin()->second.getColor(0, 0));
  EXPECT_EQ(sizes_maskable[0],
            shortcuts_menu_icons_map[0].maskable.begin()->first);
  EXPECT_EQ(
      colors_maskable[0],
      shortcuts_menu_icons_map[0].maskable.begin()->second.getColor(0, 0));
  EXPECT_EQ(sizes_any[1], shortcuts_menu_icons_map[1].any.begin()->first);
  EXPECT_EQ(colors_any[1],
            shortcuts_menu_icons_map[1].any.begin()->second.getColor(0, 0));
  EXPECT_EQ(sizes_maskable[1],
            shortcuts_menu_icons_map[1].maskable.begin()->first);
  EXPECT_EQ(
      colors_maskable[1],
      shortcuts_menu_icons_map[1].maskable.begin()->second.getColor(0, 0));
  EXPECT_EQ(sizes_any[2], shortcuts_menu_icons_map[2].any.begin()->first);
  EXPECT_EQ(colors_any[2],
            shortcuts_menu_icons_map[2].any.begin()->second.getColor(0, 0));
  EXPECT_EQ(sizes_maskable[2],
            shortcuts_menu_icons_map[2].maskable.begin()->first);
  EXPECT_EQ(
      colors_maskable[2],
      shortcuts_menu_icons_map[2].maskable.begin()->second.getColor(0, 0));
}

TEST_F(WebAppIconManagerTest, WriteShortcutsMenuIconsEmptyMap) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  web_app->SetDownloadedShortcutsMenuIconsSizes(
      CreateDownloadedShortcutsMenuIconsSizes(std::vector<SquareSizePx>{}));

  controller().RegisterApp(std::move(web_app));

  ShortcutsMenuIconBitmaps shortcuts_menu_icons;
  base::RunLoop run_loop;
  icon_manager().WriteShortcutsMenuIconsData(
      app_id, std::move(shortcuts_menu_icons),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Make sure that nothing was written to disk.
  ShortcutsMenuIconBitmaps shortcuts_menu_icons_map =
      ReadAllShortcutsMenuIcons(app_id);
  EXPECT_EQ(0u, shortcuts_menu_icons_map.size());
}

TEST_F(WebAppIconManagerTest, ReadIconsFailed) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<SquareSizePx> icon_sizes_px{icon_size::k256};

  // Set icon meta-info but don't write bitmap to disk.
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, icon_sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(
      icon_manager().HasIcons(app_id, IconPurpose::ANY, {icon_size::k96}));
  EXPECT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::ANY, {icon_size::k256}));
  EXPECT_FALSE(icon_manager().HasIcons(app_id, IconPurpose::ANY,
                                       {icon_size::k96, icon_size::k256}));

  // Request existing icon size which doesn't exist on disk.
  base::RunLoop run_loop;

  icon_manager().ReadIcons(
      app_id, IconPurpose::ANY, icon_sizes_px,
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            EXPECT_TRUE(icon_bitmaps.empty());
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, FindExact) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {40}));
  EXPECT_FALSE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {20}));

  {
    base::RunLoop run_loop;

    EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {20}));

    icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, {20},
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(1u, icon_bitmaps.size());
              EXPECT_FALSE(icon_bitmaps[20].empty());
              EXPECT_EQ(SK_ColorBLUE, icon_bitmaps[20].getColor(0, 0));
              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

// Simple struct doesn't have an operator==.
bool operator==(const IconSizeAndPurpose& a, const IconSizeAndPurpose& b) {
  return a.size_px == b.size_px && a.purpose == b.purpose;
}

TEST_F(WebAppIconManagerTest, FindSmallest) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  // Pretend we only have one size of maskable icon.
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {20});

  controller().RegisterApp(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasSmallestIcon(app_id, {IconPurpose::ANY}, 70));
  EXPECT_EQ(base::nullopt,
            icon_manager().FindIconMatchBigger(app_id, {IconPurpose::ANY}, 70));

  EXPECT_FALSE(icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 70));
  EXPECT_EQ(base::nullopt,
            icon_manager().FindIconMatchBigger(
                app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 70));

  EXPECT_FALSE(
      icon_manager().HasSmallestIcon(app_id, {IconPurpose::MASKABLE}, 40));
  EXPECT_EQ(base::nullopt, icon_manager().FindIconMatchBigger(
                               app_id, {IconPurpose::MASKABLE}, 40));

  EXPECT_TRUE(icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, 40));
  EXPECT_EQ((IconSizeAndPurpose{50, IconPurpose::ANY}),
            icon_manager()
                .FindIconMatchBigger(
                    app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, 40)
                .value());

  EXPECT_TRUE(icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 20));
  EXPECT_EQ((IconSizeAndPurpose{20, IconPurpose::ANY}),
            icon_manager()
                .FindIconMatchBigger(
                    app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 20)
                .value());

  EXPECT_TRUE(icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, 10));
  EXPECT_EQ((IconSizeAndPurpose{20, IconPurpose::MASKABLE}),
            icon_manager()
                .FindIconMatchBigger(
                    app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, 10)
                .value());

  {
    EXPECT_TRUE(icon_manager().HasSmallestIcon(app_id, {IconPurpose::ANY}, 40));
    SkBitmap bitmap = ReadSmallestIconAny(app_id, 40);
    EXPECT_FALSE(bitmap.empty());
    EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
  }
  {
    EXPECT_TRUE(icon_manager().HasSmallestIcon(app_id, {IconPurpose::ANY}, 20));
    SkBitmap bitmap = ReadSmallestIconAny(app_id, 20);
    EXPECT_FALSE(bitmap.empty());
    EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
  }
  {
    PurposeAndBitmap result =
        ReadSmallestIcon(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 20);
    EXPECT_FALSE(result.bitmap.empty());
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_EQ(SK_ColorBLUE, result.bitmap.getColor(0, 0));
  }
  {
    PurposeAndBitmap result =
        ReadSmallestIcon(app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, 20);
    EXPECT_FALSE(result.bitmap.empty());
    EXPECT_EQ(IconPurpose::MASKABLE, result.purpose);
    EXPECT_EQ(SK_ColorBLUE, result.bitmap.getColor(0, 0));
  }
}

TEST_F(WebAppIconManagerTest, DeleteData_Success) {
  const AppId app1_id = GenerateAppIdFromURL(GURL("https://example.com/"));
  const AppId app2_id = GenerateAppIdFromURL(GURL("https://example.org/"));

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorMAGENTA};
  WriteIcons(app1_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);
  WriteIcons(app2_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  const base::FilePath web_apps_root_directory =
      GetWebAppsRootDirectory(profile());
  const base::FilePath app1_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app1_id);
  const base::FilePath app2_dir =
      GetManifestResourcesDirectoryForApp(web_apps_root_directory, app2_id);

  EXPECT_TRUE(file_utils().DirectoryExists(app1_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(app1_dir));

  EXPECT_TRUE(file_utils().DirectoryExists(app2_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(app2_dir));

  base::RunLoop run_loop;
  icon_manager().DeleteData(app2_id,
                            base::BindLambdaForTesting([&](bool success) {
                              EXPECT_TRUE(success);
                              run_loop.Quit();
                            }));
  run_loop.Run();

  base::FilePath manifest_resources_directory =
      GetManifestResourcesDirectory(web_apps_root_directory);
  EXPECT_TRUE(file_utils().DirectoryExists(manifest_resources_directory));

  EXPECT_TRUE(file_utils().DirectoryExists(app1_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(app1_dir));

  EXPECT_FALSE(file_utils().DirectoryExists(app2_dir));
}

TEST_F(WebAppIconManagerTest, DeleteData_Failure) {
  const AppId app_id = GenerateAppIdFromURL(GURL("https://example.com/"));

  file_utils().SetNextDeleteFileRecursivelyResult(false);

  base::RunLoop run_loop;
  icon_manager().DeleteData(app_id,
                            base::BindLambdaForTesting([&](bool success) {
                              EXPECT_FALSE(success);
                              run_loop.Quit();
                            }));
  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, ReadSmallestCompressedIcon_Success_AnyOnly) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorGREEN};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));

  {
    PurposeAndData result =
        ReadSmallestCompressedIcon(app_id, {IconPurpose::ANY}, sizes_px[0]);
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
  {
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px[0]);
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
  {
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, sizes_px[0]);
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
}

TEST_F(WebAppIconManagerTest, ReadSmallestCompressedIcon_Success) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorGREEN};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  int size_smaller = icon_size::k64;
  int size_larger = icon_size::k128;
  // Lie about available icon sizes so any/maskable have different sizes.
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {size_smaller});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {size_larger});

  controller().RegisterApp(std::move(web_app));

  {
    PurposeAndData result =
        ReadSmallestCompressedIcon(app_id, {IconPurpose::ANY}, size_smaller);
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_FALSE(result.data.empty());

    auto* data_ptr = reinterpret_cast<const char*>(result.data.data());

    // Check that |compressed_data| starts with the 8-byte PNG magic string.
    std::string png_magic_string{data_ptr, 8};
    EXPECT_EQ(png_magic_string, "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a");
  }

  {
    // Maskable returned when purpose specified.
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::MASKABLE}, size_larger);
    EXPECT_EQ(IconPurpose::MASKABLE, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
  {
    // Maskable returned even though size doesn't exactly match.
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::MASKABLE}, size_smaller);
    EXPECT_EQ(IconPurpose::MASKABLE, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
  {
    // Any returned because it is first in purposes.
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, size_smaller);
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
  {
    // Maskable returned because it is the only one of sufficient size.
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, size_larger);
    EXPECT_EQ(IconPurpose::MASKABLE, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
  {
    // Maskable returned because it is first in purposes.
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, size_smaller);
    EXPECT_EQ(IconPurpose::MASKABLE, result.purpose);
    EXPECT_FALSE(result.data.empty());
  }
}

TEST_F(WebAppIconManagerTest, ReadSmallestCompressedIcon_Failure) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k64};
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  controller().RegisterApp(std::move(web_app));

  {
    PurposeAndData result =
        ReadSmallestCompressedIcon(app_id, {IconPurpose::ANY}, sizes_px[0]);
    EXPECT_TRUE(result.data.empty());
  }
  {
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::MASKABLE}, sizes_px[0]);
    EXPECT_TRUE(result.data.empty());
  }
  {
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px[0]);
    EXPECT_TRUE(result.data.empty());
  }
  {
    PurposeAndData result = ReadSmallestCompressedIcon(
        app_id, {IconPurpose::MASKABLE, IconPurpose::ANY}, sizes_px[0]);
    EXPECT_TRUE(result.data.empty());
  }
}

TEST_F(WebAppIconManagerTest, ReadIconAndResize_Success_AnyOnly) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k32, icon_size::k64,
                                  icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorRED};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));

  for (size_t i = 0; i < sizes_px.size(); ++i)
    EXPECT_EQ(colors[i], ReadIconAndResize(app_id, sizes_px[i]));

  // ReadIconAndResize should work for non-present icon sizes as long as an icon
  // (with matching IconPurpose) is present. It should prefer shrinking over
  // enlarging.
  EXPECT_EQ(SK_ColorYELLOW, ReadIconAndResize(app_id, icon_size::k128));
  EXPECT_EQ(SK_ColorBLUE, ReadIconAndResize(app_id, icon_size::k16));
  EXPECT_EQ(SK_ColorRED, ReadIconAndResize(app_id, 1024));

  // Maskable icons not found.
  base::RunLoop run_loop;
  icon_manager().ReadIconAndResize(
      app_id, IconPurpose::MASKABLE, icon_size::k128,
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            EXPECT_TRUE(icon_bitmaps.empty());
            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_F(WebAppIconManagerTest, ReadIconAndResize_Success_AnyAndMaskable) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k32, icon_size::k64,
                                  icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorRED};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  controller().RegisterApp(std::move(web_app));

  for (size_t i = 0; i < sizes_px.size(); ++i) {
    EXPECT_EQ(colors[i],
              ReadIconAndResize(app_id, IconPurpose::ANY, sizes_px[i]));
  }
  for (size_t i = 0; i < sizes_px.size(); ++i) {
    EXPECT_EQ(colors[i],
              ReadIconAndResize(app_id, IconPurpose::MASKABLE, sizes_px[i]));
  }

  // ReadIconAndResize should work for non-present icon sizes as long as an icon
  // (with matching IconPurpose) is present. It should prefer shrinking over
  // enlarging.
  EXPECT_EQ(SK_ColorYELLOW,
            ReadIconAndResize(app_id, IconPurpose::ANY, icon_size::k128));
  EXPECT_EQ(SK_ColorBLUE,
            ReadIconAndResize(app_id, IconPurpose::ANY, icon_size::k16));
  EXPECT_EQ(SK_ColorRED, ReadIconAndResize(app_id, IconPurpose::ANY, 1024));
  EXPECT_EQ(SK_ColorYELLOW,
            ReadIconAndResize(app_id, IconPurpose::MASKABLE, icon_size::k128));
  EXPECT_EQ(SK_ColorBLUE,
            ReadIconAndResize(app_id, IconPurpose::MASKABLE, icon_size::k16));
  EXPECT_EQ(SK_ColorRED,
            ReadIconAndResize(app_id, IconPurpose::MASKABLE, 1024));
}

TEST_F(WebAppIconManagerTest, ReadIconAndResize_Failure) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  web_app->SetDownloadedIconSizes(IconPurpose::ANY,
                                  {icon_size::k32, icon_size::k64});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE,
                                  {icon_size::k32, icon_size::k64});

  controller().RegisterApp(std::move(web_app));

  {
    base::RunLoop run_loop;
    icon_manager().ReadIconAndResize(
        app_id, IconPurpose::ANY, icon_size::k128,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_TRUE(icon_bitmaps.empty());
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    icon_manager().ReadIconAndResize(
        app_id, IconPurpose::MASKABLE, icon_size::k128,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_TRUE(icon_bitmaps.empty());
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, MatchSizes) {
  EXPECT_EQ(kWebAppIconSmall, extension_misc::EXTENSION_ICON_SMALL);
}

TEST_F(WebAppIconManagerTest, CacheExistingAppFavicon) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{gfx::kFaviconSize, icon_size::k48};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorRED};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));

  base::RunLoop run_loop;
  icon_manager().SetFaviconReadCallbackForTesting(
      base::BindLambdaForTesting([&](const AppId& cached_app_id) {
        EXPECT_EQ(cached_app_id, app_id);
        run_loop.Quit();
      }));

  icon_manager().Start();
  run_loop.Run();

  SkBitmap bitmap = icon_manager().GetFavicon(app_id);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.width());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.height());
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
}

TEST_F(WebAppIconManagerTest, CacheAppFaviconWithResize) {
  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  // App does not declare an icon of gfx::kFaviconSize, forcing a resize.
  const std::vector<int> sizes_px{8, icon_size::k48, icon_size::k64};
  ASSERT_FALSE(base::Contains(sizes_px, gfx::kFaviconSize));
  const std::vector<SkColor> colors{SK_ColorBLACK, SK_ColorGREEN, SK_ColorRED};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  controller().RegisterApp(std::move(web_app));

  base::RunLoop run_loop;
  icon_manager().SetFaviconReadCallbackForTesting(
      base::BindLambdaForTesting([&](const AppId& cached_app_id) {
        EXPECT_EQ(cached_app_id, app_id);
        run_loop.Quit();
      }));

  icon_manager().Start();
  run_loop.Run();

  SkBitmap bitmap = icon_manager().GetFavicon(app_id);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.width());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.height());
  // Correct size wasn't available so larger icon should be used.
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
}

TEST_F(WebAppIconManagerTest, CacheNewAppFavicon) {
  icon_manager().Start();

  auto web_app = CreateWebApp();
  const AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{gfx::kFaviconSize, icon_size::k48};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorRED};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  base::RunLoop run_loop;
  icon_manager().SetFaviconReadCallbackForTesting(
      base::BindLambdaForTesting([&](const AppId& cached_app_id) {
        EXPECT_EQ(cached_app_id, app_id);
        run_loop.Quit();
      }));

  controller().RegisterApp(std::move(web_app));
  registrar().NotifyWebAppInstalled(app_id);

  run_loop.Run();

  SkBitmap bitmap = icon_manager().GetFavicon(app_id);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.width());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.height());
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
}

}  // namespace web_app
