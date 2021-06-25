// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

namespace {

const char16_t kAppShortName[] = u"Test short name";
const char16_t kAppTitle[] = u"Test title";
const char16_t kAlternativeAppTitle[] = u"Different test title";
const char16_t kShortcutItemName[] = u"shortcut item ";

constexpr SquareSizePx kIconSize = 64;

// This value is greater than kMaxIcons in web_app_install_utils.cc.
constexpr unsigned int kNumTestIcons = 30;

}  // namespace

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {blink::features::kWebAppNoteTaking, blink::features::kFileHandlingIcons},
      {});

  WebApplicationInfo web_app_info;
  web_app_info.title = kAlternativeAppTitle;
  web_app_info.start_url = GURL("http://www.notchromium.org");
  WebApplicationIconInfo info;
  const GURL kAppIcon1("fav1.png");
  info.url = kAppIcon1;
  web_app_info.icon_infos.push_back(info);

  blink::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();
  manifest.short_name = kAppShortName;

  const GURL kFileHandlingIcon("fav1.png");
  {
    blink::Manifest::FileHandler handler;
    handler.action = GURL("http://example.com/open-files");
    handler.accept[u"image/png"].push_back(u".png");
    handler.name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = kFileHandlingIcon;
      icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      handler.icons.push_back(icon);
    }
    manifest.file_handlers.push_back(handler);
  }

  {
    blink::Manifest::ProtocolHandler protocol_handler;
    protocol_handler.protocol = u"mailto";
    protocol_handler.url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(protocol_handler);
  }

  {
    blink::Manifest::UrlHandler url_handler;
    url_handler.origin =
        url::Origin::Create(GURL("https://url_handlers_origin.com/"));
    url_handler.has_origin_wildcard = false;
    manifest.url_handlers.push_back(url_handler);
  }

  {
    // Ensure an empty NoteTaking struct is ignored.
    blink::Manifest::NoteTaking note_taking;
    manifest.note_taking = note_taking;
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppShortName, web_app_info.title);
  EXPECT_EQ(kAppUrl, web_app_info.start_url);
  EXPECT_EQ(kAppUrl.GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);
  EXPECT_TRUE(web_app_info.display_override.empty());
  EXPECT_EQ(kAppManifestUrl, web_app_info.manifest_url);
  EXPECT_TRUE(web_app_info.note_taking_new_note_url.is_empty());

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icon_infos.size());
  EXPECT_EQ(kAppIcon1, web_app_info.icon_infos[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = kAppTitle;
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;

  const GURL kAppIcon2("fav2.png");
  icon.src = kAppIcon2;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  const GURL kAppIcon3("fav3.png");
  icon.src = kAppIcon3;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  manifest.display_override.push_back(DisplayMode::kMinimalUi);
  manifest.display_override.push_back(DisplayMode::kStandalone);

  {
    // Update with a valid new_note_url.
    blink::Manifest::NoteTaking note_taking;
    note_taking.new_note_url = GURL("http://example.com/new-note-url");
    manifest.note_taking = note_taking;
  }

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTitle, web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  ASSERT_EQ(2u, web_app_info.display_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, web_app_info.display_override[1]);

  // We currently duplicate the app icons with multiple Purposes.
  EXPECT_EQ(5u, web_app_info.icon_infos.size());
  EXPECT_EQ(kAppIcon2, web_app_info.icon_infos[0].url);
  EXPECT_EQ(kAppIcon3, web_app_info.icon_infos[1].url);
  EXPECT_EQ(kAppIcon2, web_app_info.icon_infos[2].url);
  EXPECT_EQ(kAppIcon3, web_app_info.icon_infos[3].url);
  EXPECT_EQ(kAppIcon3, web_app_info.icon_infos[4].url);

  // Check file handlers were updated.
  EXPECT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers;
  EXPECT_EQ(manifest.file_handlers[0].action, file_handler[0].action);
  ASSERT_EQ(file_handler[0].accept.count(u"image/png"), 1u);
  EXPECT_EQ(file_handler[0].accept[u"image/png"][0], u".png");
  EXPECT_EQ(file_handler[0].name, u"Images");
  EXPECT_EQ(file_handler[0].icons.size(), 1u);
  // TODO(https://crbug.com/1218210): Consider having WebApplicationInfo's
  // file handlers use WebApplicationIconInfo, like used for other icons in
  // this layer, rather than ImageResource.
  EXPECT_EQ(file_handler[0].icons[0].src, kFileHandlingIcon);

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, u"mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  EXPECT_EQ(1u, web_app_info.url_handlers.size());
  auto url_handler = web_app_info.url_handlers[0];
  EXPECT_EQ(url_handler.origin,
            url::Origin::Create(GURL("https://url_handlers_origin.com/")));
  EXPECT_FALSE(url_handler.has_origin_wildcard);
  EXPECT_EQ(GURL("http://example.com/new-note-url"),
            web_app_info.note_taking_new_note_url);
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_EmptyName) {
  WebApplicationInfo web_app_info;

  blink::Manifest manifest;
  manifest.name = std::u16string();
  manifest.short_name = kAppShortName;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(kAppShortName, web_app_info.title);
}

// Test that maskable icons are parsed as separate icon_infos from the manifest.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_MaskableIcon) {
  blink::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = GURL("fav1.png");
  // Produces 2 separate icon_infos.
  icon.purpose = {Purpose::ANY, Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  WebApplicationInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(4u, web_app_info.icon_infos.size());
  std::map<IconPurpose, int> purpose_to_count;
  for (const auto& icon_info : web_app_info.icon_infos) {
    purpose_to_count[icon_info.purpose]++;
  }
  EXPECT_EQ(1, purpose_to_count[IconPurpose::ANY]);
  EXPECT_EQ(1, purpose_to_count[IconPurpose::MONOCHROME]);
  EXPECT_EQ(2, purpose_to_count[IconPurpose::MASKABLE]);
}

TEST(WebAppInstallUtils,
     UpdateWebAppInfoFromManifest_MaskableIconOnly_UsesManifestIcons) {
  blink::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = GURL("fav1.png");
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // WebApplicationInfo has existing icons (simulating found in page metadata).
  WebApplicationInfo web_app_info;
  WebApplicationIconInfo icon_info;
  web_app_info.icon_infos.push_back(icon_info);
  web_app_info.icon_infos.push_back(icon_info);

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  // Metadata icons are replaced by manifest icon.
  EXPECT_EQ(1U, web_app_info.icon_infos.size());
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_ShareTarget) {
  blink::Manifest manifest;
  WebApplicationInfo web_app_info;

  {
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share1");
    share_target.method = blink::mojom::ManifestShareTarget_Method::kPost;
    share_target.enctype =
        blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData;
    share_target.params.title = u"kTitle";
    share_target.params.text = u"kText";

    blink::Manifest::FileFilter file_filter;
    file_filter.name = u"kImages";
    file_filter.accept.push_back(u".png");
    file_filter.accept.push_back(u"image/png");
    share_target.params.files.push_back(std::move(file_filter));

    manifest.share_target = std::move(share_target);
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

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
    share_target.method = blink::mojom::ManifestShareTarget_Method::kGet;
    share_target.enctype =
        blink::mojom::ManifestShareTarget_Enctype::kFormUrlEncoded;
    share_target.params.text = u"kText";
    share_target.params.url = u"kUrl";

    manifest.share_target = std::move(share_target);
  }

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

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

  manifest.share_target = absl::nullopt;
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_FALSE(web_app_info.share_target.has_value());
}

// Tests that WebAppInfo is correctly updated when Manifest contains Shortcuts.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestWithShortcuts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kDesktopPWAsAppIconShortcutsMenu,
                                 blink::features::kFileHandlingIcons},
                                {});

  WebApplicationInfo web_app_info;
  web_app_info.title = kAlternativeAppTitle;
  web_app_info.start_url = GURL("http://www.notchromium.org");
  WebApplicationIconInfo info;
  const GURL kAppIcon1("fav1.png");
  info.url = kAppIcon1;
  web_app_info.icon_infos.push_back(info);

  const GURL kShortcutItemUrl("http://www.chromium.org/shortcuts/action");
  for (int i = 0; i < 4; ++i) {
    WebApplicationShortcutsMenuItemInfo shortcuts_menu_item_info;
    WebApplicationShortcutsMenuItemInfo::Icon icon;
    shortcuts_menu_item_info.name =
        kShortcutItemName + base::NumberToString16(i + 1);
    shortcuts_menu_item_info.url = kShortcutItemUrl;

    icon.url = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.square_size_px = kIconSize;

    IconPurpose purpose = kIconPurposes[i % kIconPurposes.size()];
    shortcuts_menu_item_info.SetShortcutIconInfosForPurpose(purpose,
                                                            {std::move(icon)});
    web_app_info.shortcuts_menu_item_infos.push_back(
        std::move(shortcuts_menu_item_info));
  }

  blink::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();
  manifest.short_name = kAppShortName;

  const GURL kFileHandlingIcon("fav1.png");
  {
    blink::Manifest::FileHandler handler;
    handler.action = GURL("http://example.com/open-files");
    handler.accept[u"image/png"].push_back(u".png");
    handler.name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = kFileHandlingIcon;
      icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      handler.icons.push_back(icon);
    }
    manifest.file_handlers.push_back(handler);
  }

  {
    blink::Manifest::ProtocolHandler protocol_handler;
    protocol_handler.protocol = u"mailto";
    protocol_handler.url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(protocol_handler);
  }

  {
    blink::Manifest::UrlHandler url_handler;
    url_handler.origin =
        url::Origin::Create(GURL("https://url_handlers_origin.com/"));
    url_handler.has_origin_wildcard = true;
    manifest.url_handlers.push_back(url_handler);
  }
  WebApplicationInfo web_app_info_original{web_app_info};

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppShortName, web_app_info.title);
  EXPECT_EQ(kAppUrl, web_app_info.start_url);
  EXPECT_EQ(kAppUrl.GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icon_infos.size());
  EXPECT_EQ(kAppIcon1, web_app_info.icon_infos[0].url);

  // The shortcuts_menu_item_infos from |web_app_info| should be left as is,
  // since the manifest doesn't have any shortcut information.
  EXPECT_EQ(4u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(web_app_info_original.shortcuts_menu_item_infos,
            web_app_info.shortcuts_menu_item_infos);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = kAppTitle;
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;

  const GURL kAppIcon2("fav2.png");
  icon.src = kAppIcon2;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  const GURL kAppIcon3("fav3.png");
  icon.src = kAppIcon3;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Test that shortcuts in the manifest replace those in |web_app_info|.
  blink::Manifest::ShortcutItem shortcut_item;
  shortcut_item.name = std::u16string(kShortcutItemName) + u"4";
  shortcut_item.url = kShortcutItemUrl;

  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  icon.src = kIconUrl2;
  icon.sizes.emplace_back(10, 10);
  icon.purpose = {Purpose::ANY};
  shortcut_item.icons.push_back(icon);

  manifest.shortcuts.push_back(shortcut_item);

  shortcut_item.name = std::u16string(kShortcutItemName) + u"5";

  const GURL kIconUrl3("http://www.chromium.org/shortcuts/icon3.png");
  icon.src = kIconUrl3;
  icon.purpose = {Purpose::MASKABLE, Purpose::MONOCHROME};

  shortcut_item.icons.clear();
  shortcut_item.icons.push_back(icon);

  manifest.shortcuts.push_back(shortcut_item);

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTitle, web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  // Sanity check that original copy was not changed.
  EXPECT_EQ(4u, web_app_info_original.shortcuts_menu_item_infos.size());

  // We currently duplicate the app icons with multiple Purposes.
  EXPECT_EQ(5u, web_app_info.icon_infos.size());
  EXPECT_EQ(kAppIcon2, web_app_info.icon_infos[0].url);
  EXPECT_EQ(kAppIcon3, web_app_info.icon_infos[1].url);
  EXPECT_EQ(kAppIcon2, web_app_info.icon_infos[2].url);
  EXPECT_EQ(kAppIcon3, web_app_info.icon_infos[3].url);
  EXPECT_EQ(kAppIcon3, web_app_info.icon_infos[4].url);

  EXPECT_EQ(2u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[0]
                    .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                    .size());
  WebApplicationShortcutsMenuItemInfo::Icon web_app_shortcut_icon =
      web_app_info.shortcuts_menu_item_infos[0].GetShortcutIconInfosForPurpose(
          IconPurpose::ANY)[0];
  EXPECT_EQ(kIconUrl2, web_app_shortcut_icon.url);

  EXPECT_EQ(0u, web_app_info.shortcuts_menu_item_infos[1]
                    .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                    .size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[1]
                    .GetShortcutIconInfosForPurpose(IconPurpose::MONOCHROME)
                    .size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[1]
                    .GetShortcutIconInfosForPurpose(IconPurpose::MASKABLE)
                    .size());
  web_app_shortcut_icon =
      web_app_info.shortcuts_menu_item_infos[1].GetShortcutIconInfosForPurpose(
          IconPurpose::MONOCHROME)[0];
  EXPECT_EQ(kIconUrl3, web_app_shortcut_icon.url);

  // Check file handlers were updated.
  EXPECT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers;
  EXPECT_EQ(manifest.file_handlers[0].action, file_handler[0].action);
  ASSERT_EQ(file_handler[0].accept.count(u"image/png"), 1u);
  EXPECT_EQ(file_handler[0].accept[u"image/png"][0], u".png");
  EXPECT_EQ(file_handler[0].name, u"Images");
  EXPECT_EQ(file_handler[0].icons.size(), 1u);
  // TODO(https://crbug.com/1218210): Consider having WebApplicationInfo's
  // file handlers use WebApplicationIconInfo, like used for other icons in
  // this layer, rather than ImageResource.
  EXPECT_EQ(file_handler[0].icons[0].src, kFileHandlingIcon);

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, u"mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  // Check URL handlers were updated.
  EXPECT_EQ(1u, web_app_info.url_handlers.size());
  auto url_handler = web_app_info.url_handlers[0];
  EXPECT_EQ(url_handler.origin,
            url::Origin::Create(GURL("https://url_handlers_origin.com/")));
  EXPECT_TRUE(url_handler.has_origin_wildcard);
}

// Tests that we limit the number of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyIcons) {
  blink::Manifest manifest;
  for (int i = 0; i < 50; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("fav1.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(i, i);
    manifest.icons.push_back(std::move(icon));
  }
  WebApplicationInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(20U, web_app_info.icon_infos.size());
}

// Tests that we limit the number of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyShortcutIcons) {
  base::test::ScopedFeatureList feature_list(
      features::kDesktopPWAsAppIconShortcutsMenu);

  blink::Manifest manifest;
  for (unsigned int i = 0; i < kNumTestIcons; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemName + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");

    blink::Manifest::ImageResource icon;
    icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.sizes.emplace_back(i, i);
    icon.purpose.emplace_back(IconPurpose::ANY);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  std::vector<WebApplicationShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info :
         shortcut.GetShortcutIconInfosForPurpose(IconPurpose::ANY)) {
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
    icon.src = GURL("fav1.png");
    icon.purpose.push_back(Purpose::ANY);
    const int size = i * 100;
    icon.sizes.emplace_back(size, size);
    manifest.icons.push_back(std::move(icon));
  }

  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  EXPECT_EQ(10U, web_app_info.icon_infos.size());
  for (const WebApplicationIconInfo& icon : web_app_info.icon_infos) {
    EXPECT_LE(icon.square_size_px, 1024);
  }
}

// Tests that we limit the size of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestShortcutIconsTooLarge) {
  base::test::ScopedFeatureList feature_list(
      features::kDesktopPWAsAppIconShortcutsMenu);

  blink::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemName + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");

    blink::Manifest::ImageResource icon;
    icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.purpose.push_back(Purpose::ANY);
    const int size = i * 100;
    icon.sizes.emplace_back(size, size);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebApplicationInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  std::vector<WebApplicationShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info :
         shortcut.GetShortcutIconInfosForPurpose(IconPurpose::ANY)) {
      all_icons.push_back(icon_info);
    }
  }
  EXPECT_EQ(10U, all_icons.size());
}

// Tests that SkBitmaps associated with shortcut item icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateShortcutItemIcons) {
  WebApplicationInfo web_app_info;
  WebApplicationShortcutsMenuItemInfo::Icon icon;

  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  {
    WebApplicationShortcutsMenuItemInfo shortcut_item;
    std::vector<WebApplicationShortcutsMenuItemInfo::Icon> shortcut_icon_infos;
    shortcut_item.name = std::u16string(kShortcutItemName) + u"1";
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");
    icon.url = kIconUrl1;
    icon.square_size_px = kIconSize;
    shortcut_icon_infos.push_back(icon);
    shortcut_item.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_icon_infos));
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }

  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  {
    WebApplicationShortcutsMenuItemInfo shortcut_item;
    std::vector<WebApplicationShortcutsMenuItemInfo::Icon> shortcut_icon_infos;
    shortcut_item.name = std::u16string(kShortcutItemName) + u"2";
    icon.url = kIconUrl1;
    icon.square_size_px = kIconSize;
    shortcut_icon_infos.push_back(icon);
    icon.url = kIconUrl2;
    icon.square_size_px = 2 * kIconSize;
    shortcut_icon_infos.push_back(icon);
    shortcut_item.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_icon_infos));
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
    PopulateShortcutItemIcons(&web_app_info, &icons_map);
  }

  // Ensure that reused shortcut icons are processed correctly.
  EXPECT_EQ(1U, web_app_info.shortcuts_menu_icon_bitmaps[0].any.size());
  EXPECT_EQ(0U, web_app_info.shortcuts_menu_icon_bitmaps[0].maskable.size());
  EXPECT_EQ(2U, web_app_info.shortcuts_menu_icon_bitmaps[1].any.size());
  EXPECT_EQ(0U, web_app_info.shortcuts_menu_icon_bitmaps[1].maskable.size());
}

// Tests that when PopulateShortcutItemIcons is called with no shortcut icon
// urls specified, no data is written to shortcuts_menu_item_infos.
TEST(WebAppInstallUtils, PopulateShortcutItemIconsNoShortcutIcons) {
  WebApplicationInfo web_app_info;
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon2.png"), bmp2);
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon3.png"), bmp3);

  PopulateShortcutItemIcons(&web_app_info, &icons_map);

  EXPECT_EQ(0U, web_app_info.shortcuts_menu_item_infos.size());
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with no
// app icon or shortcut icon data in web_app_info, web_app_info.icon_bitmaps_any
// is correctly populated.
TEST(WebAppInstallUtils,
     FilterAndResizeIconsGenerateMissingNoWebAppIconData_WithoutShortcuts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kDesktopPWAsAppIconShortcutsMenu);

  WebApplicationInfo web_app_info;
  web_app_info.title = u"App Name";

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with maskable
// icons available, web_app_info.icon_bitmaps_{any,maskable} are correctly
// populated.
TEST(WebAppInstallUtils, FilterAndResizeIconsGenerateMissing_MaskableIcons) {
  // Construct |icons_map| to pass to FilterAndResizeIconsGenerateMissing().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);
  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(64, SK_ColorBLUE)};
  icons_map.emplace(kIconUrl2, bmp2);

  // Construct |web_app_info| to pass icon infos.
  WebApplicationInfo web_app_info;
  web_app_info.title = u"App Name";
  WebApplicationIconInfo info;
  // Icon at URL 1 has both ANY and MASKABLE purpose.
  info.url = kIconUrl1;
  info.purpose = Purpose::ANY;
  web_app_info.icon_infos.push_back(info);
  info.purpose = Purpose::MASKABLE;
  web_app_info.icon_infos.push_back(info);
  // Icon at URL 2 has MASKABLE purpose only.
  info.url = kIconUrl2;
  info.purpose = Purpose::MASKABLE;
  web_app_info.icon_infos.push_back(info);

  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  // Expect only icon at URL 1 to be used and resized as.
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
  EXPECT_EQ(2u, web_app_info.icon_bitmaps.maskable.size());
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with maskable
// icons only, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils,
     FilterAndResizeIconsGenerateMissing_MaskableIconsOnly) {
  // Construct |icons_map| to pass to FilterAndResizeIconsGenerateMissing().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);

  // Construct |web_app_info| to pass icon infos.
  WebApplicationInfo web_app_info;
  web_app_info.title = u"App Name";
  WebApplicationIconInfo info;
  info.url = kIconUrl1;
  info.purpose = Purpose::MASKABLE;
  web_app_info.icon_infos.push_back(info);

  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_InvalidManifestUrl) {
  WebApplicationInfo web_app_info;
  blink::Manifest manifest;

  UpdateWebAppInfoFromManifest(manifest, GURL("foo"), &web_app_info);
  EXPECT_TRUE(web_app_info.manifest_url.is_empty());
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with no
// app icon or shortcut icon data in web_app_info, and kDesktopPWAShortcutsMenu
// feature enabled, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils,
     FilterAndResizeIconsGenerateMissingNoWebAppIconData_WithShortcuts) {
  base::test::ScopedFeatureList feature_list(
      features::kDesktopPWAsAppIconShortcutsMenu);

  WebApplicationInfo web_app_info;
  web_app_info.title = u"App Name";

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

// Tests that when FilterAndResizeIconsGenerateMissing is called with both
// app icon and shortcut icon bitmaps in icons_map,
// web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils, FilterAndResizeIconsGenerateMissingWithShortcutIcons) {
  base::test::ScopedFeatureList feature_list(
      features::kDesktopPWAsAppIconShortcutsMenu);

  // Construct |icons_map| to pass to FilterAndResizeIconsGenerateMissing().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);
  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(kIconSize, SK_ColorBLUE)};
  icons_map.emplace(kIconUrl2, bmp2);

  // Construct |info| to add to |web_app_info.icon_infos|.
  WebApplicationInfo web_app_info;
  web_app_info.title = u"App Name";

  WebApplicationIconInfo info;
  info.url = kIconUrl1;
  web_app_info.icon_infos.push_back(info);

  // Construct |shortcuts_menu_item_info| to add to
  // |web_app_info.shortcuts_menu_item_infos|.
  WebApplicationShortcutsMenuItemInfo shortcuts_menu_item_info;
  shortcuts_menu_item_info.name = kShortcutItemName;
  shortcuts_menu_item_info.url =
      GURL("http://www.chromium.org/shortcuts/action");
  // Construct |icon| to add to |shortcuts_menu_item_info.shortcut_icon_infos|.
  WebApplicationShortcutsMenuItemInfo::Icon icon;
  icon.url = kIconUrl2;
  icon.square_size_px = kIconSize;
  shortcuts_menu_item_info.SetShortcutIconInfosForPurpose(IconPurpose::ANY,
                                                          {std::move(icon)});
  web_app_info.shortcuts_menu_item_infos.push_back(
      std::move(shortcuts_menu_item_info));
  // Construct shortcut_icon_bitmap to add to
  // |web_app_info.shortcuts_menu_icon_bitmaps|.
  IconBitmaps shortcut_icon_bitmaps;
  shortcut_icon_bitmaps.any[kIconSize] =
      CreateSquareIcon(kIconSize, SK_ColorBLUE);
  web_app_info.shortcuts_menu_icon_bitmaps.emplace_back(shortcut_icon_bitmaps);

  FilterAndResizeIconsGenerateMissing(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

}  // namespace web_app
