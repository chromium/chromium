// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_utils.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/model/display_override.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/liburlpattern/part.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

namespace {

const char16_t kShortcutItemTestName[] = u"shortcut item ";
constexpr SquareSizePx kIconSize = 64;

GURL StartUrl() {
  return GURL("https://www.example.com/index.html");
}

// Returns a stack-allocated WebAppInstallInfo with `StartUrl()` as the
// start_url and manifest_id. Needed to migrate existing tests from the default
// constructor. Prefer to instead use
// WebAppInstallInfo::CreateWithStartUrlForTesting when adding new tests.
WebAppInstallInfo CreateWebAppInstallInfo() {
  return WebAppInstallInfo(GenerateManifestIdFromStartUrlOnly(StartUrl()),
                           StartUrl());
}

// Returns a stack-allocated WebAppInstallInfo. Needed to migrate existing tests
// from the default constructor. Prefer to instead use
// WebAppInstallInfo::CreateWithStartUrlForTesting when adding new tests.
WebAppInstallInfo CreateWebAppInstallInfoFromStartUrl(const GURL& start_url) {
  return WebAppInstallInfo(GenerateManifestIdFromStartUrlOnly(start_url),
                           start_url);
}

}  // namespace

// Tests that SkBitmaps associated with shortcut item icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateShortcutItemIcons) {
  auto web_app_info = CreateWebAppInstallInfo();
  WebAppShortcutsMenuItemInfo::Icon icon;

  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  {
    WebAppShortcutsMenuItemInfo shortcut_item;
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
    shortcut_item.name = std::u16string(kShortcutItemTestName) + u"1";
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");
    icon.url = kIconUrl1;
    icon.square_size_px = kIconSize;
    shortcut_manifest_icons.push_back(icon);
    shortcut_item.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_manifest_icons));
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }

  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  {
    WebAppShortcutsMenuItemInfo shortcut_item;
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
    shortcut_item.name = std::u16string(kShortcutItemTestName) + u"2";
    icon.url = kIconUrl1;
    icon.square_size_px = kIconSize;
    shortcut_manifest_icons.push_back(icon);
    icon.url = kIconUrl2;
    icon.square_size_px = 2 * kIconSize;
    shortcut_manifest_icons.push_back(icon);
    shortcut_item.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_manifest_icons));
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }

  {
    IconsMap icons_map;
    std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
    std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
    std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
    icons_map.emplace(kIconUrl1, bmp1);
    icons_map.emplace(kIconUrl2, bmp2);
    icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon3.png"),
                      bmp3);
    PopulateOtherIcons(&web_app_info, icons_map);
  }

  // Ensure that reused shortcut icons are processed correctly.
  EXPECT_EQ(1U, web_app_info.shortcuts_menu_icon_bitmaps[0].any.size());
  EXPECT_EQ(0U, web_app_info.shortcuts_menu_icon_bitmaps[0].maskable.size());
  EXPECT_EQ(2U, web_app_info.shortcuts_menu_icon_bitmaps[1].any.size());
  EXPECT_EQ(0U, web_app_info.shortcuts_menu_icon_bitmaps[1].maskable.size());
}

// Tests that when PopulateOtherItemIcons is called with no shortcut icon
// urls specified, no data is written to shortcuts_menu_item_infos.
TEST(WebAppInstallUtils, PopulateShortcutItemIconsNoShortcutIcons) {
  auto web_app_info = CreateWebAppInstallInfo();
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon2.png"), bmp2);
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon3.png"), bmp3);

  PopulateOtherIcons(&web_app_info, icons_map);

  EXPECT_EQ(0U, web_app_info.shortcuts_menu_item_infos.size());
}

// Tests that when PopulateProductIcons is called with maskable
// icons available, web_app_info.icon_bitmaps_{any,maskable} are correctly
// populated.
TEST(WebAppInstallUtils, PopulateProductIcons_MaskableIcons) {
  // Construct |icons_map| to pass to PopulateProductIcons().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);
  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(64, SK_ColorBLUE)};
  icons_map.emplace(kIconUrl2, bmp2);

  // Construct |web_app_info| to pass icon infos.
  auto web_app_info = CreateWebAppInstallInfo();
  web_app_info.title = u"App Name";
  apps::IconInfo info;
  // Icon at URL 1 has both kAny and kMaskable purpose.
  info.url = kIconUrl1;
  info.purpose = apps::IconInfo::Purpose::kAny;
  web_app_info.manifest_icons.push_back(info);
  info.purpose = apps::IconInfo::Purpose::kMaskable;
  web_app_info.manifest_icons.push_back(info);
  // Icon at URL 2 has kMaskable purpose only.
  info.url = kIconUrl2;
  info.purpose = apps::IconInfo::Purpose::kMaskable;
  web_app_info.manifest_icons.push_back(info);

  PopulateProductIcons(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  // Expect only icon at URL 1 to be used and resized as.
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
  EXPECT_EQ(2u, web_app_info.icon_bitmaps.maskable.size());
}

// Tests that when PopulateProductIcons is called with maskable
// icons only, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils, PopulateProductIcons_MaskableIconsOnly) {
  // Construct |icons_map| to pass to PopulateProductIcons().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);

  // Construct |web_app_info| to pass icon infos.
  auto web_app_info = CreateWebAppInstallInfo();
  web_app_info.title = u"App Name";
  apps::IconInfo info;
  info.url = kIconUrl1;
  info.purpose = apps::IconInfo::Purpose::kMaskable;
  web_app_info.manifest_icons.push_back(info);

  PopulateProductIcons(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

// Tests that when PopulateProductIcons is called with no
// app icon or shortcut icon data in web_app_info, and kDesktopPWAShortcutsMenu
// feature enabled, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils, PopulateProductIconsNoWebAppIconData_WithShortcuts) {
  auto web_app_info = CreateWebAppInstallInfo();
  web_app_info.title = u"App Name";

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  PopulateProductIcons(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

TEST(WebAppInstallUtils, PopulateProductIcons_IsGeneratedIcon) {
  {
    auto web_app_info = CreateWebAppInstallInfo();
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_TRUE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
  }
  {
    auto web_app_info = CreateWebAppInstallInfo();
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    AddIconToIconsMap(GURL("http://www.example.org/icon32.png"), icon_size::k32,
                      SK_ColorCYAN, &icons_map);

    // Does upsizing of the smallest icon.
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_FALSE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
    for (const auto& bitmap_any : web_app_info.icon_bitmaps.any) {
      EXPECT_EQ(SK_ColorCYAN, bitmap_any.second.getColor(0, 0));
    }
  }
  {
    auto web_app_info = CreateWebAppInstallInfo();
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    AddIconToIconsMap(GURL("http://www.example.org/icon512.png"),
                      icon_size::k512, SK_ColorMAGENTA, &icons_map);

    // Does downsizing of the biggest icon which is not in `SizesToGenerate()`.
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_FALSE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
    for (const auto& bitmap_any : web_app_info.icon_bitmaps.any) {
      EXPECT_EQ(SK_ColorMAGENTA, bitmap_any.second.getColor(0, 0));
    }
  }
}

// Tests that when PopulateOtherItemIcons is called with no home tab icon
// urls specified, no data is written to other_icon_bitmaps.
TEST(WebAppInstallUtils, PopulateHomeTabIconsNoHomeTabIcons_TabStrip) {
  auto web_app_info = CreateWebAppInstallInfo();
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(GURL("http://www.chromium.org/home_tab_icons/icon1.png"),
                    bmp1);
  icons_map.emplace(GURL("http://www.chromium.org/home_tab_icons/icon2.png"),
                    bmp2);
  icons_map.emplace(GURL("http://www.chromium.org/home_tab_icons/icon3.png"),
                    bmp3);

  PopulateOtherIcons(&web_app_info, icons_map);

  EXPECT_EQ(0U, web_app_info.other_icon_bitmaps.size());
}

class FileHandlersFromManifestTest : public ::testing::Test {
 protected:
  static std::vector<blink::mojom::ManifestFileHandlerPtr>
  CreateManifestFileHandlers(unsigned count) {
    std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers;
    for (unsigned i = 0; i < count; ++i) {
      auto file_handler = blink::mojom::ManifestFileHandler::New();
      file_handler->action = MakeActionUrl(i);
      file_handler->name = base::UTF8ToUTF16(base::StringPrintf("n%u", i));
      file_handler->accept[base::UTF8ToUTF16(MakeMimeType(i))] = {
          base::UTF8ToUTF16(MakeExtension(i))};
      manifest_file_handlers.push_back(std::move(file_handler));
    }
    return manifest_file_handlers;
  }

  static GURL MakeActionUrl(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("a%u", index));
  }

  static std::string MakeMimeType(unsigned index) {
    return base::StringPrintf("application/x-%u", index);
  }

  static std::string MakeExtension(unsigned index) {
    return base::StringPrintf(".e%u", index);
  }

  static GURL GetStartUrl() {
    return GURL("https://www.example.com/index.html");
  }
};

TEST_F(FileHandlersFromManifestTest, Basic) {
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers =
      CreateManifestFileHandlers(6);

  auto web_app_info = CreateWebAppInstallInfo();
  PopulateFileHandlerInfoFromManifest(manifest_file_handlers, GetStartUrl(),
                                      &web_app_info);
  const apps::FileHandlers& file_handlers = web_app_info.file_handlers;
  ASSERT_EQ(file_handlers.size(), 6U);
  for (unsigned i = 0; i < 6U; ++i) {
    EXPECT_EQ(file_handlers[i].action, MakeActionUrl(i));
    ASSERT_EQ(file_handlers[i].accept.size(), 1U);
    EXPECT_EQ(file_handlers[i].accept[0].mime_type, MakeMimeType(i));
    EXPECT_EQ(file_handlers[i].accept[0].file_extensions.size(), 1U);
    EXPECT_EQ(*file_handlers[i].accept[0].file_extensions.begin(),
              MakeExtension(i));
  }
}

// Test duplicate icon download URLs from the manifest.
TEST(WebAppInstallUtils, DuplicateIconDownloadURLs) {
  auto web_app_info = CreateWebAppInstallInfo();

  // manifest icons
  {
    apps::IconInfo info;
    info.url = GURL("http://www.chromium.org/image/icon1.png");
    web_app_info.manifest_icons.push_back(info);
  }
  {
    apps::IconInfo info;
    info.url = GURL("http://www.chromium.org/image/icon2.png");
    web_app_info.manifest_icons.push_back(info);
  }

  // shortcut icons
  {
    WebAppShortcutsMenuItemInfo shortcut_item;
    {
      std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon2.png");
        shortcut_manifest_icons.push_back(icon);
      }
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon3.png");
        shortcut_manifest_icons.push_back(icon);
      }
      shortcut_item.SetShortcutIconInfosForPurpose(
          IconPurpose::ANY, std::move(shortcut_manifest_icons));
    }
    {
      std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon3.png");
        shortcut_manifest_icons.push_back(icon);
      }
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon4.png");
        shortcut_manifest_icons.push_back(icon);
      }
      shortcut_item.SetShortcutIconInfosForPurpose(
          IconPurpose::MONOCHROME, std::move(shortcut_manifest_icons));
    }
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }
  {
    WebAppShortcutsMenuItemInfo shortcut_item;
    {
      std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon4.png");
        shortcut_manifest_icons.push_back(icon);
      }
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon5.png");
        shortcut_manifest_icons.push_back(icon);
      }
      shortcut_item.SetShortcutIconInfosForPurpose(
          IconPurpose::ANY, std::move(shortcut_manifest_icons));
    }
    {
      std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon5.png");
        shortcut_manifest_icons.push_back(icon);
      }
      {
        WebAppShortcutsMenuItemInfo::Icon icon;
        icon.url = GURL("http://www.chromium.org/image/icon6.png");
        shortcut_manifest_icons.push_back(icon);
      }
      shortcut_item.SetShortcutIconInfosForPurpose(
          IconPurpose::MASKABLE, std::move(shortcut_manifest_icons));
    }
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }

  IconUrlSizeSet download_urls = GetValidIconUrlsToDownload(web_app_info);

  const size_t download_urls_size = 6;
  EXPECT_EQ(download_urls_size, download_urls.size());
  for (size_t i = 0; i < download_urls_size; i++) {
    std::string url_str = "http://www.chromium.org/image/icon" +
                          base::NumberToString(i + 1) + ".png";
    EXPECT_EQ(1u, download_urls.count(IconUrlWithSize::CreateForUnspecifiedSize(
                      GURL(url_str))));
  }
}

TEST(WebAppInstallUtils, SetWebAppManifestFields_Summary) {
  GURL start_url("https://www.chromium.org/index.html");
  auto web_app_info = CreateWebAppInstallInfoFromStartUrl(start_url);
  web_app_info.scope = web_app_info.start_url().GetWithoutFilename();
  web_app_info.title = u"App Name";
  web_app_info.description = u"App Description";
  web_app_info.theme_color = SK_ColorCYAN;
  web_app_info.dark_mode_theme_color = SK_ColorBLACK;
  web_app_info.background_color = SK_ColorMAGENTA;
  web_app_info.dark_mode_background_color = SK_ColorBLACK;

  auto web_app = web_app::test::CreateWebApp(web_app_info.start_url());
  SetWebAppManifestFields(web_app_info, *web_app);

  EXPECT_EQ(web_app->scope(), GURL("https://www.chromium.org/"));
  EXPECT_EQ(web_app->untranslated_name(), "App Name");
  EXPECT_EQ(web_app->untranslated_description(), "App Description");
  EXPECT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(*web_app->theme_color(), SK_ColorCYAN);
  EXPECT_TRUE(web_app->dark_mode_theme_color().has_value());
  EXPECT_EQ(*web_app->dark_mode_theme_color(), SK_ColorBLACK);
  EXPECT_TRUE(web_app->background_color().has_value());
  EXPECT_EQ(*web_app->background_color(), SK_ColorMAGENTA);
  EXPECT_TRUE(web_app->dark_mode_background_color().has_value());
  EXPECT_EQ(*web_app->dark_mode_background_color(), SK_ColorBLACK);

  web_app_info.theme_color = std::nullopt;
  web_app_info.dark_mode_theme_color = std::nullopt;
  web_app_info.background_color = std::nullopt;
  web_app_info.dark_mode_background_color = std::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->theme_color().has_value());
  EXPECT_FALSE(web_app->dark_mode_theme_color().has_value());
  EXPECT_FALSE(web_app->background_color().has_value());
  EXPECT_FALSE(web_app->dark_mode_background_color().has_value());
}

TEST(WebAppInstallUtils, SetWebAppManifestFields_ShareTarget) {
  auto web_app_info = CreateWebAppInstallInfoFromStartUrl(StartUrl());
  web_app_info.scope = web_app_info.start_url().GetWithoutFilename();
  web_app_info.title = u"App Name";

  auto web_app = web_app::test::CreateWebApp(web_app_info.start_url());

  {
    apps::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share1");
    share_target.method = apps::ShareTarget::Method::kPost;
    share_target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
    share_target.params.title = "kTitle";
    share_target.params.text = "kText";

    apps::ShareTarget::Files file_filter;
    file_filter.name = "kImages";
    file_filter.accept.push_back(".png");
    file_filter.accept.push_back("image/png");
    share_target.params.files.push_back(std::move(file_filter));
    web_app_info.share_target = std::move(share_target);
  }

  SetWebAppManifestFields(web_app_info, *web_app);

  {
    EXPECT_TRUE(web_app->share_target().has_value());
    auto share_target = *web_app->share_target();
    EXPECT_EQ(share_target.action, GURL("http://example.com/share1"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kPost);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kMultipartFormData);
    EXPECT_EQ(share_target.params.title, "kTitle");
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_TRUE(share_target.params.url.empty());
    EXPECT_EQ(share_target.params.files.size(), 1U);
    EXPECT_EQ(share_target.params.files[0].name, "kImages");
    EXPECT_EQ(share_target.params.files[0].accept.size(), 2U);
    EXPECT_EQ(share_target.params.files[0].accept[0], ".png");
    EXPECT_EQ(share_target.params.files[0].accept[1], "image/png");
  }

  {
    apps::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share2");
    share_target.method = apps::ShareTarget::Method::kGet;
    share_target.enctype = apps::ShareTarget::Enctype::kFormUrlEncoded;
    share_target.params.text = "kText";
    share_target.params.url = "kUrl";
    web_app_info.share_target = std::move(share_target);
  }

  SetWebAppManifestFields(web_app_info, *web_app);

  {
    EXPECT_TRUE(web_app->share_target().has_value());
    auto share_target = *web_app->share_target();
    EXPECT_EQ(share_target.action, GURL("http://example.com/share2"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kGet);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kFormUrlEncoded);
    EXPECT_TRUE(share_target.params.title.empty());
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_EQ(share_target.params.url, "kUrl");
    EXPECT_TRUE(share_target.params.files.empty());
  }

  web_app_info.share_target = std::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->share_target().has_value());
}

TEST(WebAppInstallUtils, SetWebAppManifestFields_DisplayOverride) {
  auto web_app_info = CreateWebAppInstallInfoFromStartUrl(StartUrl());
  web_app_info.title = u"App Name";

  auto web_app = web_app::test::CreateWebApp(web_app_info.start_url());

  blink::SafeUrlPattern foo_pattern;
  foo_pattern.pathname = {
      liburlpattern::Part(liburlpattern::PartType::kFixed,
                          /*value=*/"/foo", liburlpattern::Modifier::kNone),
  };

  web_app_info.display_override = {
      DisplayOverride::CreateUnframed({foo_pattern})};

  SetWebAppManifestFields(web_app_info, *web_app);

  EXPECT_THAT(
      web_app->display_mode_override(),
      testing::ElementsAre(DisplayOverride::CreateUnframed({foo_pattern})));
}

}  // namespace web_app
