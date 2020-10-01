// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include <string>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {

using Purpose = blink::Manifest::ImageResource::Purpose;

namespace {

const char kAppShortName[] = "Test short name";
const char kAppTitle[] = "Test title";
const char kAlternativeAppTitle[] = "Different test title";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL AppIcon1() {
  return GURL("fav1.png");
}
GURL AppIcon2() {
  return GURL("fav2.png");
}
GURL AppIcon3() {
  return GURL("fav3.png");
}
GURL AppUrl() {
  return GURL("http://www.chromium.org/index.html");
}
GURL AlternativeAppUrl() {
  return GURL("http://www.notchromium.org");
}

const char kShortcutItemName[] = "shortcut item ";

GURL ShortcutItemUrl() {
  return GURL("http://www.chromium.org/shortcuts/action");
}
GURL IconUrl1() {
  return GURL("http://www.chromium.org/shortcuts/icon1.png");
}
GURL IconUrl2() {
  return GURL("http://www.chromium.org/shortcuts/icon2.png");
}
GURL IconUrl3() {
  return GURL("http://www.chromium.org/shortcuts/icon3.png");
}

constexpr SquareSizePx kIconSize = 64;

// This value is greater than kMaxIcons in web_app_install_utils.cc.
constexpr unsigned int kNumTestIcons = 30;
}  // namespace

class WebAppInstallUtilsWithShortcutsMenu : public testing::Test {
 public:
  WebAppInstallUtilsWithShortcutsMenu() {
    scoped_feature_list.InitAndEnableFeature(
        features::kDesktopPWAsAppIconShortcutsMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.start_url = AlternativeAppUrl();
  WebApplicationIconInfo info;
  info.url = AppIcon1();
  web_app_info.icon_infos.push_back(info);

  blink::Manifest manifest;
  manifest.start_url = AppUrl();
  manifest.scope = AppUrl().GetWithoutFilename();
  manifest.short_name = base::ASCIIToUTF16(kAppShortName);

  {
    blink::Manifest::FileHandler handler;
    handler.action = GURL("http://example.com/open-files");
    handler.accept[base::UTF8ToUTF16("image/png")].push_back(
        base::UTF8ToUTF16(".png"));
    handler.name = base::UTF8ToUTF16("Images");
    manifest.file_handlers.push_back(handler);
  }

  {
    blink::Manifest::ProtocolHandler protocol_handler;
    protocol_handler.protocol = base::UTF8ToUTF16("mailto");
    protocol_handler.url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(protocol_handler);
  }

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(AppUrl(), web_app_info.start_url);
  EXPECT_EQ(AppUrl().GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);
  EXPECT_TRUE(web_app_info.display_override.empty());

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon1(), web_app_info.icon_infos[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::ASCIIToUTF16(kAppTitle);
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;
  icon.src = AppIcon2();
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  icon.src = AppIcon3();
  manifest.icons.push_back(icon);
  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  manifest.display_override.push_back(DisplayMode::kMinimalUi);
  manifest.display_override.push_back(DisplayMode::kStandalone);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  ASSERT_EQ(2u, web_app_info.display_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, web_app_info.display_override[1]);

  EXPECT_EQ(2u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon2(), web_app_info.icon_infos[0].url);
  EXPECT_EQ(AppIcon3(), web_app_info.icon_infos[1].url);

  // Check file handlers were updated
  EXPECT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers;
  EXPECT_EQ(manifest.file_handlers[0].action, file_handler[0].action);
  ASSERT_EQ(file_handler[0].accept.count(base::UTF8ToUTF16("image/png")), 1u);
  EXPECT_EQ(file_handler[0].accept[base::UTF8ToUTF16("image/png")][0],
            base::UTF8ToUTF16(".png"));
  EXPECT_EQ(file_handler[0].name, base::UTF8ToUTF16("Images"));

  // Check protocol handlers were updated
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, base::UTF8ToUTF16("mailto"));
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_EmptyName) {
  WebApplicationInfo web_app_info;

  blink::Manifest manifest;
  manifest.name = base::string16();
  manifest.short_name = base::ASCIIToUTF16(kAppShortName);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
}

// Test that maskable icons are parsed as separate icon_infos from the manifest.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_MaskableIcon) {
  blink::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = AppIcon1();
  // Produces 2 separate icon_infos.
  icon.purpose = {Purpose::ANY, Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Not converted to an icon_info (for now).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  WebApplicationInfo web_app_info;

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(3U, web_app_info.icon_infos.size());
  std::map<IconPurpose, int> purpose_to_count;
  for (const auto& icon_info : web_app_info.icon_infos) {
    purpose_to_count[icon_info.purpose]++;
  }
  EXPECT_EQ(1, purpose_to_count[IconPurpose::ANY]);
  EXPECT_EQ(0, purpose_to_count[IconPurpose::MONOCHROME]);
  EXPECT_EQ(2, purpose_to_count[IconPurpose::MASKABLE]);
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_ShareTarget) {
  blink::Manifest manifest;
  WebApplicationInfo web_app_info;

  {
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share1");
    share_target.method = blink::Manifest::ShareTarget::Method::kPost;
    share_target.enctype =
        blink::Manifest::ShareTarget::Enctype::kMultipartFormData;
    share_target.params.title = base::ASCIIToUTF16("kTitle");
    share_target.params.text = base::ASCIIToUTF16("kText");

    blink::Manifest::FileFilter file_filter;
    file_filter.name = base::ASCIIToUTF16("kImages");
    file_filter.accept.push_back(base::ASCIIToUTF16(".png"));
    file_filter.accept.push_back(base::ASCIIToUTF16("image/png"));
    share_target.params.files.push_back(std::move(file_filter));

    manifest.share_target = std::move(share_target);
  }

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  {
    EXPECT_TRUE(web_app_info.share_target.has_value());
    const auto& share_target = *web_app_info.share_target;
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
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share2");
    share_target.method = blink::Manifest::ShareTarget::Method::kGet;
    share_target.enctype =
        blink::Manifest::ShareTarget::Enctype::kFormUrlEncoded;
    share_target.params.text = base::ASCIIToUTF16("kText");
    share_target.params.url = base::ASCIIToUTF16("kUrl");

    manifest.share_target = std::move(share_target);
  }

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  {
    EXPECT_TRUE(web_app_info.share_target.has_value());
    const auto& share_target = *web_app_info.share_target;
    EXPECT_EQ(share_target.action, GURL("http://example.com/share2"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kGet);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kFormUrlEncoded);
    EXPECT_TRUE(share_target.params.title.empty());
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_EQ(share_target.params.url, "kUrl");
    EXPECT_TRUE(share_target.params.files.empty());
  }

  manifest.share_target = base::nullopt;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_FALSE(web_app_info.share_target.has_value());
}

// Tests that WebAppInfo is correctly updated when Manifest contains Shortcuts.
TEST_F(WebAppInstallUtilsWithShortcutsMenu,
       UpdateWebAppInfoFromManifestWithShortcuts) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.start_url = AlternativeAppUrl();
  WebApplicationIconInfo info;
  info.url = AppIcon1();
  web_app_info.icon_infos.push_back(info);

  for (int i = 1; i < 4; ++i) {
    WebApplicationShortcutsMenuItemInfo shortcuts_menu_item_info;
    WebApplicationShortcutsMenuItemInfo::Icon icon;
    std::string shortcut_name = kShortcutItemName;
    shortcut_name += base::NumberToString(i);
    shortcuts_menu_item_info.name = base::UTF8ToUTF16(shortcut_name);
    shortcuts_menu_item_info.url = ShortcutItemUrl();

    icon.url = IconUrl1();
    icon.square_size_px = kIconSize;
    shortcuts_menu_item_info.shortcut_icon_infos.push_back(std::move(icon));
    web_app_info.shortcuts_menu_item_infos.push_back(shortcuts_menu_item_info);
  }

  blink::Manifest manifest;
  manifest.start_url = AppUrl();
  manifest.scope = AppUrl().GetWithoutFilename();
  manifest.short_name = base::ASCIIToUTF16(kAppShortName);

  {
    blink::Manifest::FileHandler handler;
    handler.action = GURL("http://example.com/open-files");
    handler.accept[base::UTF8ToUTF16("image/png")].push_back(
        base::UTF8ToUTF16(".png"));
    handler.name = base::UTF8ToUTF16("Images");
    manifest.file_handlers.push_back(handler);
  }

  {
    blink::Manifest::ProtocolHandler protocol_handler;
    protocol_handler.protocol = base::UTF8ToUTF16("mailto");
    protocol_handler.url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(protocol_handler);
  }

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(AppUrl(), web_app_info.start_url);
  EXPECT_EQ(AppUrl().GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon1(), web_app_info.icon_infos[0].url);

  // The shortcuts_menu_item_infos from |web_app_info| should be left as is,
  // since the manifest doesn't have any shortcut information.
  EXPECT_EQ(3u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(
      1u, web_app_info.shortcuts_menu_item_infos[0].shortcut_icon_infos.size());

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::ASCIIToUTF16(kAppTitle);
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;
  icon.src = AppIcon2();
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  icon.src = AppIcon3();
  manifest.icons.push_back(icon);
  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Test that shortcuts in the manifest replace those in |web_app_info|.
  blink::Manifest::ShortcutItem shortcut_item;
  std::string shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(4);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
  shortcut_item.url = ShortcutItemUrl();

  icon.src = IconUrl2();
  icon.sizes.emplace_back(10, 10);
  shortcut_item.icons.push_back(std::move(icon));

  manifest.shortcuts.push_back(shortcut_item);

  shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(5);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);

  icon.src = IconUrl3();
  shortcut_item.icons.clear();
  shortcut_item.icons.push_back(std::move(icon));

  manifest.shortcuts.push_back(shortcut_item);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);

  EXPECT_EQ(2u, web_app_info.icon_infos.size());
  EXPECT_EQ(AppIcon2(), web_app_info.icon_infos[0].url);
  EXPECT_EQ(AppIcon3(), web_app_info.icon_infos[1].url);

  EXPECT_EQ(2u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(
      1u, web_app_info.shortcuts_menu_item_infos[0].shortcut_icon_infos.size());
  WebApplicationShortcutsMenuItemInfo::Icon web_app_shortcut_icon =
      web_app_info.shortcuts_menu_item_infos[0].shortcut_icon_infos[0];
  EXPECT_EQ(IconUrl2(), web_app_shortcut_icon.url);

  EXPECT_EQ(
      1u, web_app_info.shortcuts_menu_item_infos[1].shortcut_icon_infos.size());
  web_app_shortcut_icon =
      web_app_info.shortcuts_menu_item_infos[1].shortcut_icon_infos[0];
  EXPECT_EQ(IconUrl3(), web_app_shortcut_icon.url);

  // Check file handlers were updated
  EXPECT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers;
  EXPECT_EQ(manifest.file_handlers[0].action, file_handler[0].action);
  ASSERT_EQ(file_handler[0].accept.count(base::UTF8ToUTF16("image/png")), 1u);
  EXPECT_EQ(file_handler[0].accept[base::UTF8ToUTF16("image/png")][0],
            base::UTF8ToUTF16(".png"));
  EXPECT_EQ(file_handler[0].name, base::UTF8ToUTF16("Images"));

  // Check protocol handlers were updated
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, base::UTF8ToUTF16("mailto"));
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));
}

// Tests that we limit the number of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyIcons) {
  blink::Manifest manifest;
  for (int i = 0; i < 50; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = AppIcon1();
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(i, i);
    manifest.icons.push_back(std::move(icon));
  }
  WebApplicationInfo web_app_info;

  UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(20U, web_app_info.icon_infos.size());
}

// Tests that we limit the number of shortcut icons declared by a site.
TEST_F(WebAppInstallUtilsWithShortcutsMenu,
       UpdateWebAppInfoFromManifestTooManyShortcutIcons) {
  blink::Manifest manifest;
  for (unsigned int i = 0; i < kNumTestIcons; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    std::string shortcut_name = kShortcutItemName;
    shortcut_name += base::NumberToString(i);
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = ShortcutItemUrl();

    blink::Manifest::ImageResource icon;
    icon.src = IconUrl1();
    icon.sizes.emplace_back(i, i);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  std::vector<WebApplicationShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info : shortcut.shortcut_icon_infos) {
      all_icons.push_back(icon_info);
    }
  }
  ASSERT_GT(kNumTestIcons, all_icons.size());
  EXPECT_EQ(20U, all_icons.size());
}

// Tests that we limit the size of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestIconsTooLarge) {
  blink::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = AppIcon1();
    icon.purpose.push_back(Purpose::ANY);
    const int size = i * 100;
    icon.sizes.emplace_back(size, size);
    manifest.icons.push_back(std::move(icon));
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  EXPECT_EQ(10U, web_app_info.icon_infos.size());
  for (const WebApplicationIconInfo& icon : web_app_info.icon_infos) {
    EXPECT_LE(icon.square_size_px, 1024);
  }
}

// Tests that we limit the size of shortcut icons declared by a site.
TEST_F(WebAppInstallUtilsWithShortcutsMenu,
       UpdateWebAppInfoFromManifestShortcutIconsTooLarge) {
  blink::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    std::string shortcut_name = kShortcutItemName;
    shortcut_name += base::NumberToString(i);
    shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
    shortcut_item.url = ShortcutItemUrl();

    blink::Manifest::ImageResource icon;
    icon.src = IconUrl1();
    const int size = i * 100;
    icon.sizes.emplace_back(size, size);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(manifest, &web_app_info);

  std::vector<WebApplicationShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info : shortcut.shortcut_icon_infos) {
      all_icons.push_back(icon_info);
    }
  }
  EXPECT_EQ(10U, all_icons.size());
}

// Tests that SkBitmaps associated with shortcut item icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateShortcutItemIcons) {
  WebApplicationInfo web_app_info;
  WebApplicationShortcutsMenuItemInfo shortcut_item;
  WebApplicationShortcutsMenuItemInfo::Icon icon;
  std::string shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(1);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
  shortcut_item.url = ShortcutItemUrl();
  icon.url = IconUrl1();
  icon.square_size_px = kIconSize;
  shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
  web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));

  shortcut_name = kShortcutItemName;
  shortcut_name += base::NumberToString(2);
  shortcut_item.name = base::UTF8ToUTF16(shortcut_name);
  icon.url = IconUrl1();
  icon.square_size_px = kIconSize;
  shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
  icon.url = IconUrl2();
  icon.square_size_px = 2 * kIconSize;
  shortcut_item.shortcut_icon_infos.push_back(std::move(icon));
  web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(IconUrl1(), bmp1);
  icons_map.emplace(IconUrl2(), bmp2);
  icons_map.emplace(AppIcon3(), bmp3);
  PopulateShortcutItemIcons(&web_app_info, &icons_map);

  // Ensure that reused shortcut icons are processed correctly.
  EXPECT_EQ(1U, web_app_info.shortcuts_menu_icons_bitmaps[0].size());
  EXPECT_EQ(2U, web_app_info.shortcuts_menu_icons_bitmaps[1].size());
}

// Tests that when PopulateShortcutItemIcons is called with no shortcut icon
// urls specified, no data is written to shortcuts_menu_item_infos.
TEST(WebAppInstallUtils, PopulateShortcutItemIconsNoShortcutIcons) {
  WebApplicationInfo web_app_info;
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(IconUrl1(), bmp1);
  icons_map.emplace(IconUrl2(), bmp2);
  icons_map.emplace(IconUrl3(), bmp3);

  PopulateShortcutItemIcons(&web_app_info, &icons_map);

  EXPECT_EQ(0U, web_app_info.shortcuts_menu_item_infos.size());
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with no
// app icon or shortcut icon data in web_app_info, web_app_info.icon_bitmaps_any
// is correctly populated.
TEST(WebAppInstallUtils, FilterAndResizeIconsGenerateMissingNoWebAppIconData) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::ASCIIToUTF16("App Name");

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(IconUrl1(), bmp1);
  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps_any.size());
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with maskable
// icons available, web_app_info.icon_bitmaps_{any,maskable} are correctly
// populated.
TEST(WebAppInstallUtils, FilterAndResizeIconsGenerateMissing_MaskableIcons) {
  // Construct |icons_map| to pass to FilterAndResizeIconsGenerateMissing().
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(IconUrl1(), bmp1);
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(64, SK_ColorBLUE)};
  icons_map.emplace(IconUrl2(), bmp2);

  // Construct |web_app_info| to pass icon infos.
  WebApplicationInfo web_app_info;
  web_app_info.title = base::ASCIIToUTF16("App Name");
  WebApplicationIconInfo info;
  // Icon at URL 1 has both ANY and MASKABLE purpose.
  info.url = IconUrl1();
  info.purpose = Purpose::ANY;
  web_app_info.icon_infos.push_back(info);
  info.purpose = Purpose::MASKABLE;
  web_app_info.icon_infos.push_back(info);
  // Icon at URL 2 has MASKABLE purpose only.
  info.url = IconUrl2();
  info.purpose = Purpose::MASKABLE;
  web_app_info.icon_infos.push_back(info);

  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps_any.size());
  // Expect only icon at URL 1 to be used and resized as.
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps_any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
  EXPECT_EQ(2u, web_app_info.icon_bitmaps_maskable.size());
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with maskable
// icons only, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils,
     FilterAndResizeIconsGenerateMissing_MaskableIconsOnly) {
  // Construct |icons_map| to pass to FilterAndResizeIconsGenerateMissing().
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(IconUrl1(), bmp1);

  // Construct |web_app_info| to pass icon infos.
  WebApplicationInfo web_app_info;
  web_app_info.title = base::ASCIIToUTF16("App Name");
  WebApplicationIconInfo info;
  info.url = IconUrl1();
  info.purpose = Purpose::MASKABLE;
  web_app_info.icon_infos.push_back(info);

  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps_any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps_any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with no
// app icon or shortcut icon data in web_app_info, and kDesktopPWAShortcutsMenu
// feature enabled, web_app_info.icon_bitmaps_any is correctly populated.
TEST_F(WebAppInstallUtilsWithShortcutsMenu,
       FilterAndResizeIconsGenerateMissingNoWebAppIconData) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::ASCIIToUTF16("App Name");

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(IconUrl1(), bmp1);
  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps_any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps_any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with both
// app icon and shortcut icon bitmaps in icons_map,
// web_app_info.icon_bitmaps_any is correctly populated.
TEST_F(WebAppInstallUtilsWithShortcutsMenu,
       FilterAndResizeIconsGenerateMissingWithShortcutIcons) {
  // Construct |icons_map| to pass to FilterAndResizeIconsGenerateMissing().
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(kIconSize, SK_ColorBLUE)};
  icons_map.emplace(IconUrl1(), bmp1);
  icons_map.emplace(IconUrl2(), bmp2);

  // Construct |info| to add to |web_app_info.icon_infos|.
  WebApplicationInfo web_app_info;
  web_app_info.title = base::ASCIIToUTF16("App Name");

  WebApplicationIconInfo info;
  info.url = IconUrl1();
  web_app_info.icon_infos.push_back(info);

  // Construct |shortcuts_menu_item_info| to add to
  // |web_app_info.shortcuts_menu_item_infos|.
  WebApplicationShortcutsMenuItemInfo shortcuts_menu_item_info;
  shortcuts_menu_item_info.name = base::UTF8ToUTF16(kShortcutItemName);
  shortcuts_menu_item_info.url = ShortcutItemUrl();
  // Construct |icon| to add to |shortcuts_menu_item_info.shortcut_icon_infos|.
  WebApplicationShortcutsMenuItemInfo::Icon icon;
  icon.url = IconUrl2();
  icon.square_size_px = kIconSize;
  shortcuts_menu_item_info.shortcut_icon_infos.push_back(std::move(icon));
  web_app_info.shortcuts_menu_item_infos.push_back(
      std::move(shortcuts_menu_item_info));
  // Construct shortcut_icon_bitmap to add to
  // |web_app_info.shortcuts_menu_icons_bitmaps|.
  std::map<SquareSizePx, SkBitmap> shortcut_icon_bitmaps;
  shortcut_icon_bitmaps[kIconSize] = CreateSquareIcon(kIconSize, SK_ColorBLUE);
  web_app_info.shortcuts_menu_icons_bitmaps.emplace_back(shortcut_icon_bitmaps);

  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps_any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps_any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

}  // namespace web_app
