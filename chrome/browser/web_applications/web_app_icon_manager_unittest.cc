// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/favicon_size.h"

namespace web_app {

namespace {

using IconSizeAndPurpose = WebAppIconManager::IconSizeAndPurpose;
const int kMinimumHomeTabIconSizeInPx = 16;

// Returns a vector of item infos for the shortcuts menu. Each item is empty
// except `downloaded_icon_sizes` is set to `icon_sizes`.
std::vector<WebAppShortcutsMenuItemInfo> CreateShortcutsMenuItemInfos(
    int num_menu_items,
    const IconSizes& icon_sizes) {
  std::vector<WebAppShortcutsMenuItemInfo> result;

  for (int i = 0; i < num_menu_items; ++i) {
    result.emplace_back();
    result.back().downloaded_icon_sizes = icon_sizes;
  }

  return result;
}

}  // namespace

class WebAppIconManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  void WriteGeneratedShortcutsMenuIcons(
      const webapps::AppId& app_id,
      const std::vector<GeneratedIconsInfo>& icons_info,
      int num_menu_items) {
    ShortcutsMenuIconBitmaps shortcuts_menu_icons;

    for (int i = 0; i < num_menu_items; ++i) {
      IconBitmaps menu_item_icon_map;

      for (const GeneratedIconsInfo& info : icons_info) {
        DCHECK_EQ(info.sizes_px.size(), info.colors.size());

        std::map<SquareSizePx, SkBitmap> generated_bitmaps;

        for (size_t j = 0; j < info.sizes_px.size(); ++j) {
          AddGeneratedIcon(&generated_bitmaps, info.sizes_px[j],
                           info.colors[j]);
        }

        menu_item_icon_map.SetBitmapsForPurpose(info.purpose,
                                                std::move(generated_bitmaps));
      }

      shortcuts_menu_icons.push_back(std::move(menu_item_icon_map));
    }

    base::RunLoop run_loop;
    icon_manager().WriteData(app_id, {}, std::move(shortcuts_menu_icons), {},
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  ShortcutsMenuIconBitmaps ReadAllShortcutsMenuIcons(
      const webapps::AppId& app_id) {
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

  struct PurposeAndBitmap {
    IconPurpose purpose;
    SkBitmap bitmap;
  };
  PurposeAndBitmap ReadSmallestIcon(const webapps::AppId& app_id,
                                    const std::vector<IconPurpose>& purposes,
                                    SquareSizePx min_icon_size) {
    PurposeAndBitmap result;
    base::RunLoop run_loop;
    icon_manager().ReadSmallestIcon(
        app_id, purposes, min_icon_size,
        base::BindLambdaForTesting([&](IconPurpose purpose, SkBitmap bitmap) {
          result.purpose = purpose;
          result.bitmap = std::move(bitmap);
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
      const webapps::AppId& app_id,
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

  SkColor ReadIconAndResize(const webapps::AppId& app_id,
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

  SkColor ReadIconAndResize(const webapps::AppId& app_id,
                            int desired_icon_size) {
    return ReadIconAndResize(app_id, IconPurpose::ANY, desired_icon_size);
  }

  void AddAppToRegistry(std::unique_ptr<WebApp> web_app) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  // Read favicons on web_app installation and await
  // WebAppIconManager::favicon_read_callback_ synchronously.
  void AwaitReadFaviconOnAddingWebApp(std::unique_ptr<WebApp> web_app) {
    const webapps::AppId& app_id = web_app->app_id();
    base::RunLoop run_loop;
    icon_manager().SetFaviconReadCallbackForTesting(
        base::BindLambdaForTesting([&](const webapps::AppId& cached_app_id) {
          EXPECT_EQ(cached_app_id, app_id);
          run_loop.Quit();
        }));

    AddAppToRegistry(std::move(web_app));
    install_manager().NotifyWebAppInstalled(app_id);
    run_loop.Run();
  }

  // Read Monochrome favicons on web_app installation and await
  // WebAppIconManager::favicon_monochrome_read_callback_ synchronously.
  void AwaitReadFaviconMonochromeOnAddingWebApp(
      std::unique_ptr<WebApp> web_app) {
    const webapps::AppId& app_id = web_app->app_id();
    base::RunLoop run_loop;
    icon_manager().SetFaviconMonochromeReadCallbackForTesting(
        base::BindLambdaForTesting([&](const webapps::AppId& cached_app_id) {
          EXPECT_EQ(cached_app_id, app_id);
          run_loop.Quit();
        }));

    AddAppToRegistry(std::move(web_app));
    install_manager().NotifyWebAppInstalled(app_id);
    run_loop.Run();
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }
  WebAppInstallManager& install_manager() {
    return fake_provider().install_manager();
  }
  WebAppSyncBridge& sync_bridge() {
    return fake_provider().sync_bridge_unsafe();
  }
  WebAppIconManager& icon_manager() { return fake_provider().icon_manager(); }
  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }
};

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_AnyOnly) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::MASKABLE, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  AddAppToRegistry(std::move(web_app));

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

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_MonochromeOnly) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k128, icon_size::k256};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorTRANSPARENT};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::MONOCHROME, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, sizes_px);

  AddAppToRegistry(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));
  EXPECT_FALSE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));
  EXPECT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MONOCHROME, sizes_px));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::MONOCHROME, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k128].empty());
              EXPECT_EQ(SK_ColorGREEN,
                        icon_bitmaps[icon_size::k128].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k256].empty());
              EXPECT_EQ(SK_ColorTRANSPARENT,
                        icon_bitmaps[icon_size::k256].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_AnyAndMaskable) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  AddAppToRegistry(std::move(web_app));

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

TEST_F(WebAppIconManagerTest, WriteAndReadIcons_AnyAndMonochrome) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px_any{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors_any{SK_ColorGREEN, SK_ColorYELLOW};

  const std::vector<int> sizes_px_monochrome{icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors_monochrome{SK_ColorRED, SK_ColorBLUE};

  IconManagerWriteGeneratedIcons(
      icon_manager(), app_id,
      {{IconPurpose::ANY, sizes_px_any, colors_any},
       {IconPurpose::MONOCHROME, sizes_px_monochrome, colors_monochrome}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px_any);
  web_app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, sizes_px_monochrome);

  AddAppToRegistry(std::move(web_app));

  EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px_any));
  EXPECT_FALSE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE,
                                       sizes_px_monochrome));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, sizes_px_any,
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
  EXPECT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::MONOCHROME,
                                      sizes_px_monochrome));
  {
    base::RunLoop run_loop;

    icon_manager().ReadIcons(
        app_id, IconPurpose::MONOCHROME, sizes_px_monochrome,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              EXPECT_EQ(2u, icon_bitmaps.size());

              EXPECT_FALSE(icon_bitmaps[icon_size::k64].empty());
              EXPECT_EQ(SK_ColorRED,
                        icon_bitmaps[icon_size::k64].getColor(0, 0));

              EXPECT_FALSE(icon_bitmaps[icon_size::k128].empty());
              EXPECT_EQ(SK_ColorBLUE,
                        icon_bitmaps[icon_size::k128].getColor(0, 0));

              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_F(WebAppIconManagerTest, OverwriteIcons) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // Write initial red icons to be overwritten.
  {
    std::vector<int> sizes_px{icon_size::k32, icon_size::k64, icon_size::k48};
    const std::vector<SkColor> colors{SK_ColorRED, SK_ColorRED, SK_ColorRED};
    IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                   {{IconPurpose::ANY, sizes_px, colors},
                                    {IconPurpose::MASKABLE, sizes_px, colors}});

    web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
    web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, std::move(sizes_px));
  }

  AddAppToRegistry(std::move(web_app));

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
    icon_manager().WriteData(app_id, std::move(icon_bitmaps), {}, {},
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));

    run_loop.Run();

    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AddAppToRegistry(std::move(web_app));
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

TEST_F(WebAppIconManagerTest, ReadAllIconsLastUpdateTime) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AddAppToRegistry(std::move(web_app));
  base::test::TestFuture<base::flat_map<SquareSizePx, base::Time>> future;
  {
    icon_manager().ReadIconsLastUpdateTime(app_id, future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }
  base::flat_map<SquareSizePx, base::Time> time_data_map = future.Get();
  EXPECT_EQ(2u, time_data_map.size());
  EXPECT_FALSE(time_data_map[sizes_px[0]].is_null());
  EXPECT_FALSE(time_data_map[sizes_px[1]].is_null());
}

TEST_F(WebAppIconManagerTest, ReadAllShortcutMenuIconsWithTimestamp) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};

  WriteGeneratedShortcutsMenuIcons(app_id,
                                   {{IconPurpose::ANY, sizes, colors},
                                    {IconPurpose::MASKABLE, sizes, colors},
                                    {IconPurpose::MONOCHROME, sizes, colors}},
                                   num_menu_items);

  IconSizes icon_sizes;
  icon_sizes.any = sizes;
  icon_sizes.maskable = sizes;
  icon_sizes.monochrome = sizes;

  web_app->SetShortcutsMenuInfo(
      CreateShortcutsMenuItemInfos(num_menu_items, icon_sizes));

  AddAppToRegistry(std::move(web_app));

  WebAppIconManager::ShortcutIconDataVector time_data_map;
  base::test::TestFuture<WebAppIconManager::ShortcutIconDataVector> future;
  {
    icon_manager().ReadAllShortcutMenuIconsWithTimestamp(app_id,
                                                         future.GetCallback());
    time_data_map = future.Get();
  }

  ASSERT_THAT(time_data_map.size(), num_menu_items);

  ASSERT_FALSE(time_data_map[0][IconPurpose::ANY][icon_size::k64].is_null());
  ASSERT_FALSE(time_data_map[0][IconPurpose::ANY][icon_size::k128].is_null());
  ASSERT_FALSE(
      time_data_map[0][IconPurpose::MASKABLE][icon_size::k64].is_null());
  ASSERT_FALSE(
      time_data_map[0][IconPurpose::MASKABLE][icon_size::k128].is_null());
  ASSERT_FALSE(
      time_data_map[0][IconPurpose::MONOCHROME][icon_size::k64].is_null());
  ASSERT_FALSE(
      time_data_map[0][IconPurpose::MONOCHROME][icon_size::k128].is_null());

  ASSERT_FALSE(time_data_map[1][IconPurpose::ANY][icon_size::k64].is_null());
  ASSERT_FALSE(time_data_map[1][IconPurpose::ANY][icon_size::k128].is_null());
  ASSERT_FALSE(
      time_data_map[1][IconPurpose::MASKABLE][icon_size::k64].is_null());
  ASSERT_FALSE(
      time_data_map[1][IconPurpose::MASKABLE][icon_size::k128].is_null());
  ASSERT_FALSE(
      time_data_map[1][IconPurpose::MONOCHROME][icon_size::k64].is_null());
  ASSERT_FALSE(
      time_data_map[1][IconPurpose::MONOCHROME][icon_size::k128].is_null());
}

TEST_F(WebAppIconManagerTest, NoHomeTabIcons) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  const std::vector<blink::Manifest::ImageResource>& icons{};
  SkBitmap result;

  AddAppToRegistry(std::move(web_app));
  {
    base::RunLoop run_loop;

    icon_manager().ReadBestHomeTabIcon(
        app_id, icons, kMinimumHomeTabIconSizeInPx,
        base::BindLambdaForTesting([&](SkBitmap home_tab_icon) {
          result = std::move(home_tab_icon);
          run_loop.Quit();
        }));

    run_loop.Run();
    EXPECT_TRUE(result.height() == 0 && result.width() == 0);
  }
}

TEST_F(WebAppIconManagerTest, ReadAllIcons_AnyAndMaskable) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  AddAppToRegistry(std::move(web_app));
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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<SquareSizePx> sizes_px_any{icon_size::k96, icon_size::k256};
  const int num_menu_items = 10;

  IconSizes icon_sizes;
  icon_sizes.any = sizes_px_any;

  // Set shortcuts menu icons meta-info but don't write bitmaps to disk.
  web_app->SetShortcutsMenuInfo(
      CreateShortcutsMenuItemInfos(num_menu_items, icon_sizes));

  AddAppToRegistry(std::move(web_app));

  // Request shortcuts menu icons which don't exist on disk.
  ShortcutsMenuIconBitmaps shortcuts_menu_icons_map =
      ReadAllShortcutsMenuIcons(app_id);
  EXPECT_EQ(10u, shortcuts_menu_icons_map.size());
  for (const auto& icon_map : shortcuts_menu_icons_map) {
    EXPECT_TRUE(icon_map.empty());
  }
}

TEST_F(WebAppIconManagerTest, WriteAndReadAllShortcutsMenuIcons) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const int num_menu_items = 3;

  const std::vector<int> sizes_any = {icon_size::k64, icon_size::k128,
                                      icon_size::k256};
  const std::vector<SkColor> colors_any = {SK_ColorRED, SK_ColorWHITE,
                                           SK_ColorBLUE};

  const std::vector<int> sizes_maskable = {icon_size::k64, icon_size::k96,
                                           icon_size::k128};
  const std::vector<SkColor> colors_maskable = {SK_ColorCYAN, SK_ColorMAGENTA,
                                                SK_ColorYELLOW};

  const std::vector<int> sizes_monochrome = {icon_size::k64, icon_size::k96,
                                             icon_size::k128};
  const std::vector<SkColor> colors_monochrome = {SK_ColorGREEN, SK_ColorBLACK,
                                                  SK_ColorTRANSPARENT};

  WriteGeneratedShortcutsMenuIcons(
      app_id,
      {{IconPurpose::ANY, sizes_any, colors_any},
       {IconPurpose::MASKABLE, sizes_maskable, colors_maskable},
       {IconPurpose::MONOCHROME, sizes_monochrome, colors_monochrome}},
      num_menu_items);

  IconSizes icon_sizes;
  icon_sizes.any = sizes_any;
  icon_sizes.maskable = sizes_maskable;
  icon_sizes.monochrome = sizes_monochrome;

  web_app->SetShortcutsMenuInfo(
      CreateShortcutsMenuItemInfos(num_menu_items, icon_sizes));

  AddAppToRegistry(std::move(web_app));

  ShortcutsMenuIconBitmaps shortcuts_menu_icons_map =
      ReadAllShortcutsMenuIcons(app_id);
  EXPECT_EQ(3u, shortcuts_menu_icons_map.size());

  for (int i = 0; i < num_menu_items; ++i) {
    for (IconPurpose purpose : kIconPurposes) {
      SCOPED_TRACE(purpose);

      const std::vector<int>* expect_sizes;
      const std::vector<SkColor>* expect_colors;

      switch (purpose) {
        case IconPurpose::ANY:
          expect_sizes = &sizes_any;
          expect_colors = &colors_any;
          break;
        case IconPurpose::MASKABLE:
          expect_sizes = &sizes_maskable;
          expect_colors = &colors_maskable;
          break;
        case IconPurpose::MONOCHROME:
          expect_sizes = &sizes_monochrome;
          expect_colors = &colors_monochrome;
          break;
      }

      const std::map<SquareSizePx, SkBitmap>& icon_bitmaps =
          shortcuts_menu_icons_map[i].GetBitmapsForPurpose(purpose);

      ASSERT_EQ(expect_sizes->size(), expect_colors->size());

      for (unsigned s = 0; s < expect_sizes->size(); ++s) {
        const SquareSizePx size_px = (*expect_sizes)[s];

        const auto& size_and_bitmap = icon_bitmaps.find(size_px);
        ASSERT_TRUE(size_and_bitmap != icon_bitmaps.end());

        EXPECT_EQ((*expect_colors)[s], size_and_bitmap->second.getColor(0, 0));
      }
    }
  }
}

TEST_F(WebAppIconManagerTest, WriteNonProductIconsEmptyMaps) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  web_app->SetShortcutsMenuInfo({});

  AddAppToRegistry(std::move(web_app));

  base::RunLoop run_loop;
  icon_manager().WriteData(app_id, {}, {}, {},
                           base::BindLambdaForTesting([&](bool success) {
                             EXPECT_TRUE(success);
                             run_loop.Quit();
                           }));
  run_loop.Run();

  // Make sure that nothing was written to disk.
  ShortcutsMenuIconBitmaps shortcuts_menu_icons_map =
      ReadAllShortcutsMenuIcons(app_id);
  EXPECT_EQ(0u, shortcuts_menu_icons_map.size());

  EXPECT_FALSE(file_utils().PathExists(GetOtherIconsDir(profile(), app_id)));
  // TODO(estade): check that WebAppIconManager returns no data when other icons
  // are read. (When there is a read function.)
}

TEST_F(WebAppIconManagerTest, WriteOtherIconsToDisk) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  AddAppToRegistry(std::move(web_app));

  IconsMap other_icons;
  const GURL example_gurl("https://example.com/image.png");
  AddIconToIconsMap(example_gurl, 48, SK_ColorBLUE, &other_icons);
  base::RunLoop run_loop;
  icon_manager().WriteData(app_id, {}, {}, other_icons,
                           base::BindLambdaForTesting([&](bool success) {
                             EXPECT_TRUE(success);
                             run_loop.Quit();
                           }));
  run_loop.Run();

  base::FilePath other_icons_dir = GetOtherIconsDir(profile(), app_id);
  EXPECT_TRUE(file_utils().DirectoryExists(other_icons_dir));
  EXPECT_FALSE(file_utils().IsDirectoryEmpty(other_icons_dir));
  // TODO(estade): check that WebAppIconManager returns correct data when other
  // icons are read. (When there is a read function.)
}

TEST_F(WebAppIconManagerTest, ReadIconsFailed) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<SquareSizePx> icon_sizes_px{icon_size::k256};

  // Set icon meta-info but don't write bitmap to disk.
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, icon_sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{10, 60, 50, 20, 30};
  const std::vector<SkColor> colors{SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN,
                                    SK_ColorBLUE, SK_ColorMAGENTA};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  // Pretend we only have one size of maskable icon.
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {20});

  AddAppToRegistry(std::move(web_app));

  EXPECT_FALSE(icon_manager().HasSmallestIcon(app_id, {IconPurpose::ANY}, 70));
  EXPECT_EQ(std::nullopt,
            icon_manager().FindIconMatchBigger(app_id, {IconPurpose::ANY}, 70));

  EXPECT_FALSE(icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 70));
  EXPECT_EQ(std::nullopt,
            icon_manager().FindIconMatchBigger(
                app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, 70));

  EXPECT_FALSE(
      icon_manager().HasSmallestIcon(app_id, {IconPurpose::MASKABLE}, 40));
  EXPECT_EQ(std::nullopt, icon_manager().FindIconMatchBigger(
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
    PurposeAndBitmap result = ReadSmallestIcon(app_id, {IconPurpose::ANY}, 40);
    EXPECT_FALSE(result.bitmap.empty());
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_EQ(SK_ColorGREEN, result.bitmap.getColor(0, 0));
  }
  {
    EXPECT_TRUE(icon_manager().HasSmallestIcon(app_id, {IconPurpose::ANY}, 20));
    PurposeAndBitmap result = ReadSmallestIcon(app_id, {IconPurpose::ANY}, 20);
    EXPECT_FALSE(result.bitmap.empty());
    EXPECT_EQ(IconPurpose::ANY, result.purpose);
    EXPECT_EQ(SK_ColorBLUE, result.bitmap.getColor(0, 0));
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
  const webapps::AppId app1_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL("https://example.com/"));
  const webapps::AppId app2_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL("https://example.org/"));

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorMAGENTA};
  IconManagerWriteGeneratedIcons(icon_manager(), app1_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});
  IconManagerWriteGeneratedIcons(icon_manager(), app2_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});

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
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL("https://example.com/"));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorGREEN};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorGREEN};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});

  int size_smaller = icon_size::k64;
  int size_larger = icon_size::k128;
  // Lie about available icon sizes so any/maskable have different sizes.
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {size_smaller});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {size_larger});

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k64};
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k32, icon_size::k64,
                                  icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{icon_size::k32, icon_size::k64,
                                  icon_size::k256, icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors},
                                  {IconPurpose::MASKABLE, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  AddAppToRegistry(std::move(web_app));

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
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  web_app->SetDownloadedIconSizes(IconPurpose::ANY,
                                  {icon_size::k32, icon_size::k64});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE,
                                  {icon_size::k32, icon_size::k64});

  AddAppToRegistry(std::move(web_app));

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

TEST_F(WebAppIconManagerTest, CacheExistingAppFavicon) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{gfx::kFaviconSize, icon_size::k48};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  SkBitmap bitmap = icon_manager().GetFavicon(app_id);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.width());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.height());
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
}

TEST_F(WebAppIconManagerTest, CacheAppFaviconWithResize) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // App does not declare an icon of gfx::kFaviconSize, forcing a resize.
  const std::vector<int> sizes_px{8, icon_size::k48, icon_size::k64};
  ASSERT_FALSE(base::Contains(sizes_px, gfx::kFaviconSize));
  const std::vector<SkColor> colors{SK_ColorBLACK, SK_ColorGREEN, SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  SkBitmap bitmap = icon_manager().GetFavicon(app_id);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.width());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.height());
  // Correct size wasn't available so larger icon should be used.
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
}

TEST_F(WebAppIconManagerTest, CacheNewAppFavicon) {
  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  const std::vector<int> sizes_px{gfx::kFaviconSize, icon_size::k48};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  SkBitmap bitmap = icon_manager().GetFavicon(app_id);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.width());
  EXPECT_EQ(gfx::kFaviconSize, bitmap.height());
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
}

TEST_F(WebAppIconManagerTest, CacheAppFavicon_UiScaleFactors_NoMissingIcons) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // App declares icons precisely matching suspported UI scale factors.
  const std::vector<int> sizes_px{icon_size::k16, icon_size::k32,
                                  icon_size::k48, icon_size::k64};
  ASSERT_TRUE(base::Contains(sizes_px, gfx::kFaviconSize));

  const std::vector<SkColor> colors{SK_ColorYELLOW, SK_ColorGREEN, SK_ColorRED,
                                    SK_ColorBLUE};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia image_skia = icon_manager().GetFaviconImageSkia(app_id);
  ASSERT_FALSE(image_skia.isNull());

  EXPECT_EQ(gfx::kFaviconSize, image_skia.width());
  EXPECT_EQ(gfx::kFaviconSize, image_skia.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(image_skia, /*scale=*/1.0f, /*size_px=*/icon_size::k16,
                       SK_ColorYELLOW);
  }
  {
    SCOPED_TRACE(icon_size::k32);
    ExpectImageSkiaRep(image_skia, /*scale=*/2.0f, /*size_px=*/icon_size::k32,
                       SK_ColorGREEN);
  }
  {
    SCOPED_TRACE(icon_size::k48);
    ExpectImageSkiaRep(image_skia, /*scale=*/3.0f, /*size_px=*/icon_size::k48,
                       SK_ColorRED);
  }
  {
    SCOPED_TRACE(icon_size::k64);
    ExpectImageSkiaRep(image_skia, /*scale=*/4.0f, /*size_px=*/icon_size::k64,
                       SK_ColorBLUE);
  }
}

TEST_F(WebAppIconManagerTest, CacheAppFavicon_UiScaleFactors_DownsizingIcons) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // App declares only bigger icons, forcing a downsize to suspported UI scale
  // factors.
  const std::vector<int> sizes_px{icon_size::k24, icon_size::k48};
  ASSERT_FALSE(base::Contains(sizes_px, gfx::kFaviconSize));

  const std::vector<SkColor> colors{SK_ColorCYAN, SK_ColorMAGENTA};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::ANY, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia image_skia = icon_manager().GetFaviconImageSkia(app_id);
  ASSERT_FALSE(image_skia.isNull());

  EXPECT_EQ(gfx::kFaviconSize, image_skia.width());
  EXPECT_EQ(gfx::kFaviconSize, image_skia.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(image_skia, /*scale=*/1.0f, /*size_px=*/icon_size::k16,
                       SK_ColorCYAN);
  }
  {
    SCOPED_TRACE(icon_size::k24);
    ExpectImageSkiaRep(image_skia, /*scale=*/1.5f, /*size_px=*/icon_size::k24,
                       SK_ColorCYAN);
  }
  {
    SCOPED_TRACE(icon_size::k48);
    ExpectImageSkiaRep(image_skia, /*scale=*/3.0f, /*size_px=*/icon_size::k48,
                       SK_ColorMAGENTA);
  }
}

TEST_F(WebAppIconManagerTest, CacheAppFavicon_UiScaleFactors_NoIcons) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia image_skia = icon_manager().GetFaviconImageSkia(app_id);
  EXPECT_TRUE(image_skia.isNull());
}

TEST_F(WebAppIconManagerTest, CacheAppFavicon_UiScaleFactors_NoMatchSmaller) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // App declares only smaller icon and implementations ignore it: no upsizing.
  const std::vector<int> sizes_px{15};
  IconManagerWriteGeneratedIcons(
      icon_manager(), app_id,
      {{IconPurpose::ANY, sizes_px, /*colors=*/{SK_ColorRED}}});
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia image_skia = icon_manager().GetFaviconImageSkia(app_id);
  EXPECT_TRUE(image_skia.isNull());
}

TEST_F(WebAppIconManagerTest,
       CacheAppFavicon_UiScaleFactors_DownsizingFromSingleIcon) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // App declares only one jumbo icon.
  const std::vector<int> sizes_px{icon_size::k512};
  IconManagerWriteGeneratedIcons(
      icon_manager(), app_id,
      {{IconPurpose::ANY, sizes_px, /*colors=*/{SK_ColorLTGRAY}}});
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia image_skia = icon_manager().GetFaviconImageSkia(app_id);
  ASSERT_FALSE(image_skia.isNull());

  EXPECT_EQ(gfx::kFaviconSize, image_skia.width());
  EXPECT_EQ(gfx::kFaviconSize, image_skia.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(image_skia, /*scale=*/1.0f, /*size_px=*/icon_size::k16,
                       SK_ColorLTGRAY);
  }
  {
    SCOPED_TRACE(icon_size::k64);
    ExpectImageSkiaRep(image_skia, /*scale=*/4.0f, /*size_px=*/icon_size::k64,
                       SK_ColorLTGRAY);
  }
  EXPECT_FALSE(image_skia.HasRepresentation(2.0f));
  EXPECT_FALSE(image_skia.HasRepresentation(3.0f));
  EXPECT_FALSE(image_skia.HasRepresentation(32.0f));
}

TEST_F(WebAppIconManagerTest,
       CacheAppFavicon_UiScaleFactors_BiggerUiScaleFactorIconMissing) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();

  // App declares the icon which is ok for 100P but small for 300P.
  const std::vector<int> sizes_px{icon_size::k32};
  IconManagerWriteGeneratedIcons(
      icon_manager(), app_id,
      {{IconPurpose::ANY, sizes_px, /*colors=*/{SK_ColorDKGRAY}}});
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  AwaitReadFaviconOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia image_skia = icon_manager().GetFaviconImageSkia(app_id);
  ASSERT_FALSE(image_skia.isNull());

  EXPECT_EQ(gfx::kFaviconSize, image_skia.width());
  EXPECT_EQ(gfx::kFaviconSize, image_skia.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(image_skia, /*scale=*/1.0f, /*size_px=*/icon_size::k16,
                       SK_ColorDKGRAY);
  }
  {
    SCOPED_TRACE(icon_size::k32);
    ExpectImageSkiaRep(image_skia, /*scale=*/2.0f, /*size_px=*/icon_size::k32,
                       SK_ColorDKGRAY);
  }
  EXPECT_FALSE(image_skia.HasRepresentation(3.0f));
}

#if BUILDFLAG(IS_CHROMEOS)
using WebAppIconManagerTest_NotificationIconAndTitle = WebAppIconManagerTest;

// TODO(b/321111988): Reenable this test.
TEST_F(WebAppIconManagerTest_NotificationIconAndTitle,
       DISABLED_CacheAppMonochromeFavicon_NoMissingIcons) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  web_app->SetThemeColor(std::make_optional(SK_ColorBLUE));

  const webapps::AppId app_id = web_app->app_id();

  // App declares icons precisely matching suspported UI scale factors.
  const std::vector<int> sizes_px{icon_size::k16, icon_size::k32,
                                  icon_size::k64};
  ASSERT_TRUE(base::Contains(sizes_px, gfx::kFaviconSize));

  const std::vector<SkColor> colors{SK_ColorYELLOW, SK_ColorTRANSPARENT,
                                    SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::MONOCHROME, sizes_px, colors}});

  web_app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, sizes_px);

  AwaitReadFaviconMonochromeOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia monochrome_image = icon_manager().GetMonochromeFavicon(app_id);
  ASSERT_FALSE(monochrome_image.isNull());

  EXPECT_EQ(gfx::kFaviconSize, monochrome_image.width());
  EXPECT_EQ(gfx::kFaviconSize, monochrome_image.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/1.0f,
                       /*size_px=*/icon_size::k16, SK_ColorBLUE);
  }
  {
    SCOPED_TRACE(icon_size::k32);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/2.0f,
                       /*size_px=*/icon_size::k32, SK_ColorTRANSPARENT);
  }
  {
    SCOPED_TRACE(icon_size::k64);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/4.0f,
                       /*size_px=*/icon_size::k64, SK_ColorBLUE);
  }
}

TEST_F(WebAppIconManagerTest_NotificationIconAndTitle,
       CacheAppMonochromeFavicon_CacheAfterAppInstall) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  web_app->SetThemeColor(std::make_optional(SK_ColorGREEN));

  const webapps::AppId app_id = web_app->app_id();

  // App declares only one jumbo icon.
  const std::vector<int> sizes_px{icon_size::k512};
  const std::vector<SkColor> colors{SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::MONOCHROME, sizes_px, colors}});
  web_app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, sizes_px);

  AwaitReadFaviconMonochromeOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia monochrome_image = icon_manager().GetMonochromeFavicon(app_id);
  ASSERT_FALSE(monochrome_image.isNull());

  EXPECT_EQ(gfx::kFaviconSize, monochrome_image.width());
  EXPECT_EQ(gfx::kFaviconSize, monochrome_image.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/1.0f,
                       /*size_px=*/icon_size::k16, SK_ColorGREEN);
  }
  {
    SCOPED_TRACE(icon_size::k64);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/4.0f,
                       /*size_px=*/icon_size::k64, SK_ColorGREEN);
  }
  EXPECT_FALSE(monochrome_image.HasRepresentation(2.0f));
  EXPECT_FALSE(monochrome_image.HasRepresentation(3.0f));
  EXPECT_FALSE(monochrome_image.HasRepresentation(32.0f));
}

TEST_F(WebAppIconManagerTest_NotificationIconAndTitle,
       CacheAppMonochromeFavicon_NoThemeColor) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  web_app->SetThemeColor(std::nullopt);

  const webapps::AppId app_id = web_app->app_id();

  // Provides only k200Percent icon.
  const std::vector<int> sizes_px{icon_size::k32};
  const std::vector<SkColor> colors{SK_ColorRED};
  IconManagerWriteGeneratedIcons(icon_manager(), app_id,
                                 {{IconPurpose::MONOCHROME, sizes_px, colors}});
  web_app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, sizes_px);

  AwaitReadFaviconMonochromeOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia monochrome_image = icon_manager().GetMonochromeFavicon(app_id);
  ASSERT_FALSE(monochrome_image.isNull());

  EXPECT_EQ(gfx::kFaviconSize, monochrome_image.width());
  EXPECT_EQ(gfx::kFaviconSize, monochrome_image.height());
  {
    SCOPED_TRACE(icon_size::k16);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/1.0f,
                       /*size_px=*/icon_size::k16, SK_ColorDKGRAY);
  }
  {
    SCOPED_TRACE(icon_size::k32);
    ExpectImageSkiaRep(monochrome_image, /*scale=*/2.0f,
                       /*size_px=*/icon_size::k32, SK_ColorDKGRAY);
  }
  EXPECT_FALSE(monochrome_image.HasRepresentation(3.0));
}

TEST_F(WebAppIconManagerTest_NotificationIconAndTitle,
       CacheAppMonochromeFavicon_NoIcons) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  AwaitReadFaviconMonochromeOnAddingWebApp(std::move(web_app));

  gfx::ImageSkia monochrome_image = icon_manager().GetMonochromeFavicon(app_id);
  EXPECT_TRUE(monochrome_image.isNull());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
