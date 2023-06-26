// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_utils.h"

#include <stddef.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"

#endif
namespace web_app {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

namespace {

const char16_t kAppTestShortName[] = u"Test short name";
const char16_t kAppTestTitle[] = u"Test title";
const char16_t kAlternativeAppTestTitle[] = u"Different test title";
const char16_t kShortcutItemTestName[] = u"shortcut item ";

constexpr SquareSizePx kIconSize = 64;

// This value is greater than kMaxIcons in web_app_install_utils.cc.
constexpr unsigned int kNumTestIcons = 30;

IconPurpose IconInfoPurposeToManifestPurpose(
    apps::IconInfo::Purpose icon_info_purpose) {
  switch (icon_info_purpose) {
    case apps::IconInfo::Purpose::kAny:
      return IconPurpose::ANY;
    case apps::IconInfo::Purpose::kMonochrome:
      return IconPurpose::MONOCHROME;
    case apps::IconInfo::Purpose::kMaskable:
      return IconPurpose::MASKABLE;
  }
}

GURL StartUrl() {
  return GURL("https://www.example.com/index.html");
}

}  // namespace

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({blink::features::kFileHandlingIcons,
                                 blink::features::kWebAppManifestLockScreen},
                                /*disabled_features=*/{});

  WebAppInstallInfo web_app_info;
  web_app_info.title = kAlternativeAppTestTitle;
  web_app_info.start_url = GURL("http://www.notchromium.org");
  apps::IconInfo info;
  const GURL kAppIcon1("fav1.png");
  info.url = kAppIcon1;
  web_app_info.manifest_icons.push_back(info);

  blink::mojom::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();
  manifest.short_name = kAppTestShortName;

  {
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://example.com/open-files");
    handler->accept[u"image/png"].push_back(u".png");
    handler->name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = GURL("fav1.png");
      icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      handler->icons.push_back(icon);
    }
    manifest.file_handlers.push_back(std::move(handler));
  }

  {
    auto protocol_handler = blink::mojom::ManifestProtocolHandler::New();
    protocol_handler->protocol = u"mailto";
    protocol_handler->url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(std::move(protocol_handler));
  }

  {
    auto url_handler = blink::mojom::ManifestUrlHandler::New();
    url_handler->origin =
        url::Origin::Create(GURL("https://url_handlers_origin.com/"));
    url_handler->has_origin_wildcard = false;
    manifest.url_handlers.push_back(std::move(url_handler));
  }

  {
    auto scope_extension = blink::mojom::ManifestScopeExtension::New();
    scope_extension->origin =
        url::Origin::Create(GURL("https://scope_extensions_origin.com/"));
    scope_extension->has_origin_wildcard = false;
    manifest.scope_extensions.push_back(std::move(scope_extension));
  }

  {
    blink::ParsedPermissionsPolicyDeclaration declaration;
    declaration.feature = blink::mojom::PermissionsPolicyFeature::kFullscreen;
    declaration.allowed_origins = {
        *blink::OriginWithPossibleWildcards::FromOrigin(
            url::Origin::Create(GURL("https://www.example.com")))};
    declaration.matches_all_origins = false;
    declaration.matches_opaque_src = false;

    manifest.permissions_policy.push_back(std::move(declaration));
  }

  {
    // Ensure empty structs are ignored.
    manifest.lock_screen = blink::mojom::ManifestLockScreen::New();
    manifest.note_taking = blink::mojom::ManifestNoteTaking::New();
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestShortName, web_app_info.title);
  EXPECT_EQ(kAppUrl, web_app_info.start_url);
  EXPECT_EQ(kAppUrl.GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);
  EXPECT_TRUE(web_app_info.display_override.empty());
  EXPECT_EQ(kAppManifestUrl, web_app_info.manifest_url);
  EXPECT_TRUE(web_app_info.lock_screen_start_url.is_empty());
  EXPECT_TRUE(web_app_info.note_taking_new_note_url.is_empty());

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon1, web_app_info.manifest_icons[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = kAppTestTitle;
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
    auto lock_screen = blink::mojom::ManifestLockScreen::New();
    lock_screen->start_url =
        GURL("http://www.chromium.org/lock-screen-start-url");
    manifest.lock_screen = std::move(lock_screen);
  }

  {
    // Update with a valid new_note_url.
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url = GURL("http://www.chromium.org/new-note-url");
    manifest.note_taking = std::move(note_taking);
  }

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestTitle, web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  ASSERT_EQ(2u, web_app_info.display_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, web_app_info.display_override[1]);

  // We currently duplicate the app icons with multiple Purposes.
  EXPECT_EQ(5u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[0].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[1].url);
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[2].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[3].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[4].url);

  // Check file handlers were updated.
  ASSERT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers[0];
  ASSERT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ(file_handler.accept[0].mime_type, "image/png");
  EXPECT_EQ(manifest.file_handlers[0]->action, file_handler.action);
  EXPECT_TRUE(file_handler.accept[0].file_extensions.contains(".png"));

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, "mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  // Check URL handlers were updated.
  EXPECT_EQ(1u, web_app_info.url_handlers.size());
  auto url_handler = web_app_info.url_handlers[0];
  EXPECT_EQ(url_handler.origin,
            url::Origin::Create(GURL("https://url_handlers_origin.com/")));
  EXPECT_FALSE(url_handler.has_origin_wildcard);

  // Check scope extensions were updated.
  EXPECT_EQ(1u, web_app_info.scope_extensions.size());
  auto scope_extension = *web_app_info.scope_extensions.begin();
  EXPECT_EQ(scope_extension.origin,
            url::Origin::Create(GURL("https://scope_extensions_origin.com/")));
  EXPECT_FALSE(scope_extension.has_origin_wildcard);

  EXPECT_EQ(GURL("http://www.chromium.org/lock-screen-start-url"),
            web_app_info.lock_screen_start_url);

  EXPECT_EQ(GURL("http://www.chromium.org/new-note-url"),
            web_app_info.note_taking_new_note_url);

  // Check permissions policy was updated.
  EXPECT_EQ(1u, web_app_info.permissions_policy.size());
  auto declaration = web_app_info.permissions_policy[0];
  EXPECT_EQ(declaration.feature,
            blink::mojom::PermissionsPolicyFeature::kFullscreen);
  EXPECT_EQ(1u, declaration.allowed_origins.size());
  EXPECT_EQ("https://www.example.com",
            declaration.allowed_origins[0].Serialize());
  EXPECT_FALSE(declaration.matches_all_origins);
  EXPECT_FALSE(declaration.matches_opaque_src);
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_EmptyName) {
  WebAppInstallInfo web_app_info;

  blink::mojom::Manifest manifest;
  manifest.name = absl::nullopt;
  manifest.short_name = kAppTestShortName;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(kAppTestShortName, web_app_info.title);
}

// Test that maskable icons are parsed as separate manifest_icons from the
// manifest.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_MaskableIcon) {
  blink::mojom::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = GURL("fav1.png");
  // Produces 2 separate manifest_icons.
  icon.purpose = {Purpose::ANY, Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  WebAppInstallInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(4u, web_app_info.manifest_icons.size());
  std::map<IconPurpose, int> purpose_to_count;
  for (const auto& icon_info : web_app_info.manifest_icons) {
    purpose_to_count[IconInfoPurposeToManifestPurpose(icon_info.purpose)]++;
  }
  EXPECT_EQ(1, purpose_to_count[IconPurpose::ANY]);
  EXPECT_EQ(1, purpose_to_count[IconPurpose::MONOCHROME]);
  EXPECT_EQ(2, purpose_to_count[IconPurpose::MASKABLE]);
}

TEST(WebAppInstallUtils,
     UpdateWebAppInfoFromManifest_MaskableIconOnly_UsesManifestIcons) {
  blink::mojom::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = GURL("fav1.png");
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // WebAppInstallInfo has existing icons (simulating found in page metadata).
  WebAppInstallInfo web_app_info;
  apps::IconInfo icon_info;
  web_app_info.manifest_icons.push_back(icon_info);
  web_app_info.manifest_icons.push_back(icon_info);

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  // Metadata icons are replaced by manifest icon.
  EXPECT_EQ(1U, web_app_info.manifest_icons.size());
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_ShareTarget) {
  blink::mojom::Manifest manifest;
  WebAppInstallInfo web_app_info;

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
  base::test::ScopedFeatureList feature_list(
      blink::features::kFileHandlingIcons);

  WebAppInstallInfo web_app_info;
  web_app_info.title = kAlternativeAppTestTitle;
  web_app_info.start_url = GURL("http://www.notchromium.org");
  apps::IconInfo info;
  const GURL kAppIcon1("fav1.png");
  info.url = kAppIcon1;
  web_app_info.manifest_icons.push_back(info);

  blink::mojom::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();
  manifest.short_name = kAppTestShortName;

  {
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://example.com/open-files");
    handler->accept[u"image/png"].push_back(u".png");
    handler->name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = GURL("fav1.png");
      icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      handler->icons.push_back(icon);
    }
    manifest.file_handlers.push_back(std::move(handler));
  }

  {
    auto protocol_handler = blink::mojom::ManifestProtocolHandler::New();
    protocol_handler->protocol = u"mailto";
    protocol_handler->url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(std::move(protocol_handler));
  }

  {
    auto url_handler = blink::mojom::ManifestUrlHandler::New();
    url_handler->origin =
        url::Origin::Create(GURL("https://url_handlers_origin.com/"));
    url_handler->has_origin_wildcard = true;
    manifest.url_handlers.push_back(std::move(url_handler));
  }

  {
    auto scope_extension = blink::mojom::ManifestScopeExtension::New();
    scope_extension->origin =
        url::Origin::Create(GURL("https://scope_extensions_origin.com/"));
    scope_extension->has_origin_wildcard = true;
    manifest.scope_extensions.push_back(std::move(scope_extension));
  }

  WebAppInstallInfo web_app_info_original{web_app_info.Clone()};

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestShortName, web_app_info.title);
  EXPECT_EQ(kAppUrl, web_app_info.start_url);
  EXPECT_EQ(kAppUrl.GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon1, web_app_info.manifest_icons[0].url);

  EXPECT_EQ(0u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(web_app_info_original.shortcuts_menu_item_infos,
            web_app_info.shortcuts_menu_item_infos);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = kAppTestTitle;
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
  const GURL kShortcutItemUrl("http://www.chromium.org/shortcuts/action");
  blink::Manifest::ShortcutItem shortcut_item;
  shortcut_item.name = std::u16string(kShortcutItemTestName) + u"4";
  shortcut_item.url = kShortcutItemUrl;

  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  icon.src = kIconUrl2;
  icon.sizes.emplace_back(10, 10);
  icon.purpose = {Purpose::ANY};
  shortcut_item.icons.push_back(icon);

  manifest.shortcuts.push_back(shortcut_item);

  shortcut_item.name = std::u16string(kShortcutItemTestName) + u"5";

  const GURL kIconUrl3("http://www.chromium.org/shortcuts/icon3.png");
  icon.src = kIconUrl3;
  icon.purpose = {Purpose::MASKABLE, Purpose::MONOCHROME};

  shortcut_item.icons.clear();
  shortcut_item.icons.push_back(icon);

  manifest.shortcuts.push_back(shortcut_item);

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestTitle, web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  // Sanity check that original copy was not changed.
  EXPECT_EQ(0u, web_app_info_original.shortcuts_menu_item_infos.size());

  // We currently duplicate the app icons with multiple Purposes.
  EXPECT_EQ(5u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[0].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[1].url);
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[2].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[3].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[4].url);

  EXPECT_EQ(2u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[0]
                    .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                    .size());
  WebAppShortcutsMenuItemInfo::Icon web_app_shortcut_icon =
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
  ASSERT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers[0];
  ASSERT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ(file_handler.accept[0].mime_type, "image/png");
  EXPECT_EQ(manifest.file_handlers[0]->action, file_handler.action);
  EXPECT_TRUE(file_handler.accept[0].file_extensions.contains(".png"));

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, "mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  // Check URL handlers were updated.
  EXPECT_EQ(1u, web_app_info.url_handlers.size());
  auto url_handler = web_app_info.url_handlers[0];
  EXPECT_EQ(url_handler.origin,
            url::Origin::Create(GURL("https://url_handlers_origin.com/")));
  EXPECT_TRUE(url_handler.has_origin_wildcard);

  // Check scope extensions were updated.
  EXPECT_EQ(1u, web_app_info.scope_extensions.size());
  auto scope_extension = *web_app_info.scope_extensions.begin();
  EXPECT_EQ(scope_extension.origin,
            url::Origin::Create(GURL("https://scope_extensions_origin.com/")));
  EXPECT_TRUE(scope_extension.has_origin_wildcard);
}

// Tests that we limit the number of shortcut menu items.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyShortcuts) {
  blink::mojom::Manifest manifest;
  const unsigned kMaxShortcuts = 10U;
  for (unsigned int i = 1; i <= kMaxShortcuts + 1; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemTestName + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");
    manifest.shortcuts.push_back(shortcut_item);
  }
  EXPECT_LT(kMaxShortcuts, manifest.shortcuts.size());
  WebAppInstallInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  EXPECT_EQ(kMaxShortcuts, web_app_info.shortcuts_menu_item_infos.size());
}

// Tests that we limit the number of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyIcons) {
  blink::mojom::Manifest manifest;
  for (unsigned int i = 0; i < kNumTestIcons; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("fav1.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(i, i);
    manifest.icons.push_back(std::move(icon));
  }
  WebAppInstallInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  ASSERT_GT(kNumTestIcons, web_app_info.manifest_icons.size());
  EXPECT_EQ(20U, web_app_info.manifest_icons.size());
}

// Tests that we limit the number of shortcut icons, verifying that at most 20
// shortcut icons are stored per web app.
//
// The test previously created 30 shortcuts, each with 1 icon. Due to the new
// limit of 10 shortcuts per web app, we now create 5 shortcuts, with 6 icons
// each.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyShortcutIcons) {
  blink::mojom::Manifest manifest;
  manifest.start_url = GURL("http://www.chromium.org/");
  manifest.id = GURL("http://www.chromium.org/");
  const unsigned kNumShortcuts = 5;

  for (unsigned int i = 0; i < kNumShortcuts; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemTestName + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");

    for (unsigned int j = 1; j <= kNumTestIcons / kNumShortcuts; ++j) {
      blink::Manifest::ImageResource icon;
      icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
      icon.sizes.emplace_back(j, j);
      icon.purpose.emplace_back(IconPurpose::ANY);
      shortcut_item.icons.push_back(std::move(icon));
    }

    manifest.shortcuts.push_back(std::move(shortcut_item));
  }
  WebAppInstallInfo web_app_info = CreateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"));

  std::vector<WebAppShortcutsMenuItemInfo::Icon> all_icons;
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
  blink::mojom::Manifest manifest;
  for (int size = 1023; size <= 1026; ++size) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("fav1.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(size, size);
    manifest.icons.push_back(std::move(icon));
  }

  WebAppInstallInfo web_app_info;
  // Icons exceeding size 1024 are discarded.
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  // Only the early icons are within the size limit.
  EXPECT_EQ(2U, web_app_info.manifest_icons.size());
  for (const apps::IconInfo& icon : web_app_info.manifest_icons) {
    EXPECT_LE(icon.square_size_px, 1024);
  }
}

// Tests that we limit the size of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestShortcutIconsTooLarge) {
  blink::mojom::Manifest manifest;
  for (int size = 1023; size <= 1026; ++size) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemTestName + base::NumberToString16(size);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");

    blink::Manifest::ImageResource icon;
    icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(size, size);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }

  WebAppInstallInfo web_app_info;
  // Icons exceeding size 1024 are discarded.
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  std::vector<WebAppShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info :
         shortcut.GetShortcutIconInfosForPurpose(IconPurpose::ANY)) {
      all_icons.push_back(icon_info);
    }
  }
  // Only the early icons are within the size limit.
  EXPECT_EQ(2U, all_icons.size());
}

TEST(WebAppInstallUtils,
     UpdateWebAppInfoFromManifest_CrossOriginUrls_DropsFields) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kWebAppManifestLockScreen);

  WebAppInstallInfo install_info;

  blink::mojom::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();

  {
    auto lock_screen = blink::mojom::ManifestLockScreen::New();
    lock_screen->start_url =
        GURL("http://www.some-other-origin.com/lock-screen-start-url");
    manifest.lock_screen = std::move(lock_screen);
  }

  {
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url =
        GURL("http://www.some-other-origin.com/new-note-url");
    manifest.note_taking = std::move(note_taking);
  }

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &install_info);

  EXPECT_EQ(kAppUrl, install_info.start_url);
  EXPECT_TRUE(install_info.lock_screen_start_url.is_empty());
  EXPECT_TRUE(install_info.note_taking_new_note_url.is_empty());
}

TEST(WebAppInstallUtils,
     UpdateWebAppInfoFromManifest_WithoutLockscreenFlag_DropsField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      blink::features::kWebAppManifestLockScreen);

  WebAppInstallInfo install_info;

  blink::mojom::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();

  {
    auto lock_screen = blink::mojom::ManifestLockScreen::New();
    lock_screen->start_url =
        GURL("http://www.chromium.org/lock-screen-start-url");
    manifest.lock_screen = std::move(lock_screen);
  }

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &install_info);

  EXPECT_EQ(kAppUrl, install_info.start_url);
  EXPECT_TRUE(install_info.lock_screen_start_url.is_empty());
}

// Tests that SkBitmaps associated with shortcut item icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateShortcutItemIcons) {
  WebAppInstallInfo web_app_info;
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
  WebAppInstallInfo web_app_info;
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
  WebAppInstallInfo web_app_info;
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
  WebAppInstallInfo web_app_info;
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

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_InvalidManifestUrl) {
  WebAppInstallInfo web_app_info;
  blink::mojom::Manifest manifest;

  UpdateWebAppInfoFromManifest(manifest, GURL("foo"), &web_app_info);
  EXPECT_TRUE(web_app_info.manifest_url.is_empty());
}

// Tests that when PopulateProductIcons is called with no
// app icon or shortcut icon data in web_app_info, and kDesktopPWAShortcutsMenu
// feature enabled, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils, PopulateProductIconsNoWebAppIconData_WithShortcuts) {
  WebAppInstallInfo web_app_info;
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
    WebAppInstallInfo web_app_info;
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_TRUE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
  }
  {
    WebAppInstallInfo web_app_info;
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    AddIconToIconsMap(GURL("http://www.example.org/icon32.png"), icon_size::k32,
                      SK_ColorCYAN, &icons_map);

    // Does upsizing of the smallest icon.
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_FALSE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
    for (const auto& bitmap_any : web_app_info.icon_bitmaps.any)
      EXPECT_EQ(SK_ColorCYAN, bitmap_any.second.getColor(0, 0));
  }
  {
    WebAppInstallInfo web_app_info;
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    AddIconToIconsMap(GURL("http://www.example.org/icon512.png"),
                      icon_size::k512, SK_ColorMAGENTA, &icons_map);

    // Does downsizing of the biggest icon which is not in `SizesToGenerate()`.
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_FALSE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
    for (const auto& bitmap_any : web_app_info.icon_bitmaps.any)
      EXPECT_EQ(SK_ColorMAGENTA, bitmap_any.second.getColor(0, 0));
  }
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_Translations) {
  blink::mojom::Manifest manifest;
  WebAppInstallInfo web_app_info;

  {
    blink::Manifest::TranslationItem item;
    item.name = "name 1";
    item.short_name = "short name 1";
    item.description = "description 1";

    manifest.translations[u"language 1"] = std::move(item);
  }
  {
    blink::Manifest::TranslationItem item;
    item.short_name = "short name 2";
    item.description = "description 2";

    manifest.translations[u"language 2"] = std::move(item);
  }
  {
    blink::Manifest::TranslationItem item;
    item.name = "name 3";

    manifest.translations[u"language 3"] = std::move(item);
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

  EXPECT_EQ(3u, web_app_info.translations.size());
  EXPECT_EQ(web_app_info.translations["language 1"].name, "name 1");
  EXPECT_EQ(web_app_info.translations["language 1"].short_name, "short name 1");
  EXPECT_EQ(web_app_info.translations["language 1"].description,
            "description 1");

  EXPECT_FALSE(web_app_info.translations["language 2"].name);
  EXPECT_EQ(web_app_info.translations["language 2"].short_name, "short name 2");
  EXPECT_EQ(web_app_info.translations["language 2"].description,
            "description 2");

  EXPECT_EQ(web_app_info.translations["language 3"].name, "name 3");
  EXPECT_FALSE(web_app_info.translations["language 3"].short_name);
  EXPECT_FALSE(web_app_info.translations["language 3"].description);
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_TabStrip) {
  blink::mojom::Manifest manifest;
  WebAppInstallInfo web_app_info;

  {
    TabStrip tab_strip;
    tab_strip.home_tab = TabStrip::Visibility::kAbsent;
    tab_strip.new_tab_button = TabStrip::Visibility::kAuto;
    manifest.tab_strip = std::move(tab_strip);

    const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
    UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

    EXPECT_TRUE(web_app_info.tab_strip.has_value());
    EXPECT_EQ(absl::get<TabStrip::Visibility>(
                  web_app_info.tab_strip.value().home_tab),
              TabStrip::Visibility::kAbsent);
    EXPECT_EQ(absl::get<TabStrip::Visibility>(
                  web_app_info.tab_strip.value().new_tab_button),
              TabStrip::Visibility::kAuto);
  }

  {
    blink::Manifest::ImageResource icon;
    const GURL kAppIcon("fav1.png");
    icon.purpose = {Purpose::ANY};
    icon.src = kAppIcon;

    TabStrip tab_strip;
    blink::Manifest::HomeTabParams home_tab_params;
    home_tab_params.icons.push_back(icon);
    tab_strip.home_tab = home_tab_params;

    blink::Manifest::NewTabButtonParams new_tab_button_params;
    new_tab_button_params.url = GURL("https://www.example.com/");
    tab_strip.new_tab_button = new_tab_button_params;
    manifest.tab_strip = std::move(tab_strip);

    const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
    UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

    EXPECT_TRUE(web_app_info.tab_strip.has_value());
    EXPECT_EQ(absl::get<blink::Manifest::HomeTabParams>(
                  web_app_info.tab_strip.value().home_tab)
                  .icons.size(),
              1u);
    EXPECT_EQ(absl::get<blink::Manifest::HomeTabParams>(
                  web_app_info.tab_strip.value().home_tab)
                  .icons[0]
                  .src,
              kAppIcon);
    EXPECT_EQ(absl::get<blink::Manifest::NewTabButtonParams>(
                  web_app_info.tab_strip.value().new_tab_button)
                  .url,
              GURL("https://www.example.com/"));
  }
}

// All home tab icons are saved from the manifest except for icons that
// exceed the maximum allowed size.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestHomeTabIcons_TabStrip) {
  blink::mojom::Manifest manifest;
  TabStrip tab_strip;
  tab_strip.home_tab = web_app::TabStrip::Visibility::kAuto;
  blink::Manifest::HomeTabParams home_tab_params;
  for (int size = 1023; size <= 1026; ++size) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.purpose.push_back(web_app::Purpose::ANY);
    icon.sizes.emplace_back(size, size);
    home_tab_params.icons.push_back(std::move(icon));
  }
  tab_strip.home_tab = home_tab_params;
  manifest.tab_strip = std::move(tab_strip);

  WebAppInstallInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_TRUE(web_app_info.tab_strip.has_value());
  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info.tab_strip.value().home_tab);
  EXPECT_EQ(2U, home_tab.icons.size());
}

// Tests that when PopulateOtherItemIcons is called with no home tab icon
// urls specified, no data is written to other_icon_bitmaps.
TEST(WebAppInstallUtils, PopulateHomeTabIconsNoHomeTabIcons_TabStrip) {
  WebAppInstallInfo web_app_info;
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

// Tests that SkBitmaps associated with home tab icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateHomeTabIcons_TabStrip) {
  WebAppInstallInfo web_app_info;

  blink::mojom::Manifest manifest;
  TabStrip tab_strip;
  tab_strip.home_tab = web_app::TabStrip::Visibility::kAuto;
  blink::Manifest::HomeTabParams home_tab_params;

  const GURL kIconUrl1("http://www.chromium.org/home_tab_icons/icon1.png");
  {
    blink::Manifest::ImageResource icon;
    icon.src = kIconUrl1;
    icon.purpose.push_back(web_app::Purpose::ANY);
    icon.sizes.emplace_back(kIconSize, kIconSize);
    home_tab_params.icons.push_back(std::move(icon));
  }

  const GURL kIconUrl2("http://www.chromium.org/home_tab_icons/icon2.png");
  {
    blink::Manifest::ImageResource icon;
    icon.src = kIconUrl1;
    icon.purpose.push_back(web_app::Purpose::ANY);
    // This icon is too big and will be filtered out
    icon.sizes.emplace_back(kIconSize * 2, kIconSize * 2);
    home_tab_params.icons.push_back(std::move(icon));
  }

  EXPECT_EQ(2U, home_tab_params.icons.size());

  tab_strip.home_tab = home_tab_params;
  manifest.tab_strip = std::move(tab_strip);

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_TRUE(web_app_info.tab_strip.has_value());
  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info.tab_strip.value().home_tab);
  EXPECT_EQ(2U, home_tab.icons.size());

  {
    IconsMap icons_map;
    std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
    std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
    std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
    icons_map.emplace(kIconUrl1, bmp1);
    icons_map.emplace(kIconUrl2, bmp2);
    icons_map.emplace(GURL("http://www.chromium.org/home_tab_icons/icon3.png"),
                      bmp3);
    PopulateOtherIcons(&web_app_info, icons_map);
  }

  // Ensure that reused home tab icons are processed correctly.
  // Icons that are too big and icons that exist in icons_map, but are not in
  // web_app_info.tab_strip are filtered out.
  EXPECT_EQ(1U, web_app_info.other_icon_bitmaps.size());
}

class FileHandlersFromManifestTest : public ::testing::TestWithParam<bool> {
 public:
  FileHandlersFromManifestTest() {
    feature_list_.InitAndEnableFeature(blink::features::kFileHandlingIcons);
    WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(GetParam());
  }

  ~FileHandlersFromManifestTest() override = default;

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

      blink::Manifest::ImageResource icon;
      icon.src = MakeImageUrl(i);
      icon.sizes = {{16, 16}, {32, 32}, {64, 64}};
      icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
      file_handler->icons.push_back(std::move(icon));

      blink::Manifest::ImageResource icon2;
      icon2.src = MakeImageUrlForSecondImage(i);
      icon2.sizes = {{16, 16}};
      icon2.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY,
                       blink::mojom::ManifestImageResource_Purpose::MASKABLE};
      file_handler->icons.push_back(std::move(icon2));

      manifest_file_handlers.push_back(std::move(file_handler));
    }
    return manifest_file_handlers;
  }

  static GURL MakeActionUrl(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("a%u", index));
  }

  static GURL MakeImageUrl(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("image%u.png", index));
  }

  static GURL MakeImageUrlForSecondImage(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("image%u-2.png", index));
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

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(FileHandlersFromManifestTest, Basic) {
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers =
      CreateManifestFileHandlers(6);

  apps::FileHandlers file_handlers =
      CreateFileHandlersFromManifest(manifest_file_handlers, GetStartUrl());
  ASSERT_EQ(file_handlers.size(), 6U);
  for (unsigned i = 0; i < 6U; ++i) {
    EXPECT_EQ(file_handlers[i].action, MakeActionUrl(i));
    ASSERT_EQ(file_handlers[i].accept.size(), 1U);
    EXPECT_EQ(file_handlers[i].accept[0].mime_type, MakeMimeType(i));
    EXPECT_EQ(file_handlers[i].accept[0].file_extensions.size(), 1U);
    EXPECT_EQ(*file_handlers[i].accept[0].file_extensions.begin(),
              MakeExtension(i));

    if (WebAppFileHandlerManager::IconsEnabled()) {
      ASSERT_EQ(file_handlers[i].downloaded_icons.size(), 3U);

      // The manifest-specified `sizes` are ignored.
      EXPECT_FALSE(file_handlers[i].downloaded_icons[0].square_size_px);
      EXPECT_EQ(MakeImageUrl(i), file_handlers[i].downloaded_icons[0].url);
      EXPECT_EQ(apps::IconInfo::Purpose::kAny,
                file_handlers[i].downloaded_icons[0].purpose);

      EXPECT_FALSE(file_handlers[i].downloaded_icons[1].square_size_px);
      EXPECT_EQ(MakeImageUrlForSecondImage(i),
                file_handlers[i].downloaded_icons[1].url);
      EXPECT_EQ(apps::IconInfo::Purpose::kAny,
                file_handlers[i].downloaded_icons[1].purpose);

      EXPECT_FALSE(file_handlers[i].downloaded_icons[2].square_size_px);
      EXPECT_EQ(MakeImageUrlForSecondImage(i),
                file_handlers[i].downloaded_icons[2].url);
      EXPECT_EQ(apps::IconInfo::Purpose::kMaskable,
                file_handlers[i].downloaded_icons[2].purpose);
    } else {
      EXPECT_TRUE(file_handlers[i].downloaded_icons.empty());
    }
  }
}

TEST_P(FileHandlersFromManifestTest, PopulateFileHandlerIcons) {
  if (!WebAppFileHandlerManager::IconsEnabled())
    return;

  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers =
      CreateManifestFileHandlers(1);
  WebAppInstallInfo web_app_info;
  web_app_info.file_handlers =
      CreateFileHandlersFromManifest(manifest_file_handlers, GetStartUrl());

  const GURL first_image_url = MakeImageUrl(0);
  const GURL second_image_url = MakeImageUrlForSecondImage(0);
  IconsMap icons_map;
  // The first URL returns two valid bitmaps and one invalid (non-square), which
  // should be ignored.
  std::vector<SkBitmap> bmps1 = {CreateSquareIcon(17, SK_ColorWHITE),
                                 CreateSquareIcon(29, SK_ColorBLUE),
                                 gfx::test::CreateBitmap(16, 15)};
  icons_map.emplace(first_image_url, bmps1);
  std::vector<SkBitmap> bmps2 = {CreateSquareIcon(79, SK_ColorRED),
                                 CreateSquareIcon(134, SK_ColorRED)};
  icons_map.emplace(second_image_url, bmps2);
  PopulateOtherIcons(&web_app_info, icons_map);

  // Make sure bitmaps are copied from `icons_map` into `web_app_info`.
  // Images downloaded from two distinct URLs.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps.size());
  // First URL correlates to two bitmaps.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps[first_image_url].size());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[first_image_url][0],
                           icons_map[first_image_url][0]));
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[first_image_url][1],
                           icons_map[first_image_url][1]));
  // Second URL correlates to two more bitmaps.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps[second_image_url].size());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[second_image_url][0],
                           icons_map[second_image_url][0]));
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[second_image_url][1],
                           icons_map[second_image_url][1]));

  // We end up with one file handler with 6 icon infos. The second URL produces
  // 4 IconInfos because it has two bitmaps and two purposes: 2 x 2 = 4.
  ASSERT_EQ(1U, web_app_info.file_handlers.size());

  // The metadata we expect to be saved after icons are finished downloading and
  // processing. Note that the icon sizes saved to `apps::FileHandler::icons`
  // match downloaded sizes, not those specified in the manifest.
  struct {
    GURL expected_url;
    apps::IconInfo::SquareSizePx expected_size;
    apps::IconInfo::Purpose expected_purpose;
  } expectations[] = {
      {first_image_url, 17, apps::IconInfo::Purpose::kAny},
      {first_image_url, 29, apps::IconInfo::Purpose::kAny},
      {second_image_url, 79, apps::IconInfo::Purpose::kAny},
      {second_image_url, 134, apps::IconInfo::Purpose::kAny},
      {second_image_url, 79, apps::IconInfo::Purpose::kMaskable},
      {second_image_url, 134, apps::IconInfo::Purpose::kMaskable},
  };

  const size_t num_expectations =
      sizeof(expectations) / sizeof(expectations[0]);
  ASSERT_EQ(num_expectations,
            web_app_info.file_handlers[0].downloaded_icons.size());

  for (size_t i = 0; i < num_expectations; ++i) {
    const auto& icon = web_app_info.file_handlers[0].downloaded_icons[i];
    EXPECT_EQ(expectations[i].expected_url, icon.url);
    EXPECT_EQ(expectations[i].expected_size, icon.square_size_px);
    EXPECT_EQ(expectations[i].expected_purpose, icon.purpose);
  }
}

// Tests both file handlers and home tab icons as they use the same variable to
// store their bitmaps.
TEST_P(FileHandlersFromManifestTest, PopulateFileHandlingAndHomeTabIcons) {
  if (!WebAppFileHandlerManager::IconsEnabled()) {
    return;
  }

  WebAppInstallInfo web_app_info;

  // Put icons in for the home tab
  blink::mojom::Manifest manifest;
  TabStrip tab_strip;
  tab_strip.home_tab = web_app::TabStrip::Visibility::kAuto;
  blink::Manifest::HomeTabParams home_tab_params;

  const GURL kHomeTabIconUrl1(
      "http://www.chromium.org/home_tab_icons/icon1.png");
  {
    blink::Manifest::ImageResource icon;
    icon.src = kHomeTabIconUrl1;
    icon.purpose.push_back(web_app::Purpose::ANY);
    icon.sizes.emplace_back(kIconSize, kIconSize);
    home_tab_params.icons.push_back(std::move(icon));
  }

  const GURL kHomeTabIconUrl2(
      "http://www.chromium.org/home_tab_icons/icon2.png");
  {
    blink::Manifest::ImageResource icon;
    icon.src = kHomeTabIconUrl1;
    icon.purpose.push_back(web_app::Purpose::ANY);
    // This icon is too big and will be filtered out
    icon.sizes.emplace_back(kIconSize * 2, kIconSize * 2);
    home_tab_params.icons.push_back(std::move(icon));
  }

  EXPECT_EQ(2U, home_tab_params.icons.size());

  tab_strip.home_tab = home_tab_params;
  manifest.tab_strip = std::move(tab_strip);

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_TRUE(web_app_info.tab_strip.has_value());
  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info.tab_strip.value().home_tab);
  EXPECT_EQ(2U, home_tab.icons.size());

  // Put icons in for file handlers
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers =
      CreateManifestFileHandlers(1);
  web_app_info.file_handlers =
      CreateFileHandlersFromManifest(manifest_file_handlers, GetStartUrl());

  const GURL kFileHandlerIconUrl1 = MakeImageUrlForSecondImage(0);
  const GURL kFileHandlerIconUrl2 = MakeImageUrl(0);

  IconsMap icons_map;

  {
    std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
    std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
    std::vector<SkBitmap> bmp3 = {CreateSquareIcon(79, SK_ColorRED),
                                  CreateSquareIcon(134, SK_ColorRED)};
    std::vector<SkBitmap> bmp4 = {CreateSquareIcon(17, SK_ColorWHITE),
                                  CreateSquareIcon(29, SK_ColorBLUE),
                                  gfx::test::CreateBitmap(16, 15)};
    icons_map.emplace(kHomeTabIconUrl1, bmp1);
    icons_map.emplace(kHomeTabIconUrl2, bmp2);
    icons_map.emplace(kFileHandlerIconUrl1, bmp3);
    icons_map.emplace(kFileHandlerIconUrl2, bmp4);
    PopulateOtherIcons(&web_app_info, icons_map);
  }

  // Ensure that reused home tab icons are processed correctly.
  // Icons that are too big and icons that exist in icons_map, but are not in
  // web_app_info.tab_strip are filtered out.
  EXPECT_EQ(3U, web_app_info.other_icon_bitmaps.size());

  // Fourth URL correlates to two bitmaps.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps[kFileHandlerIconUrl2].size());
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      web_app_info.other_icon_bitmaps[kFileHandlerIconUrl2][0],
      icons_map[kFileHandlerIconUrl2][0]));
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      web_app_info.other_icon_bitmaps[kFileHandlerIconUrl2][1],
      icons_map[kFileHandlerIconUrl2][1]));
  // Third URL correlates to two more bitmaps.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps[kFileHandlerIconUrl1].size());
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      web_app_info.other_icon_bitmaps[kFileHandlerIconUrl1][0],
      icons_map[kFileHandlerIconUrl1][0]));
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      web_app_info.other_icon_bitmaps[kFileHandlerIconUrl1][1],
      icons_map[kFileHandlerIconUrl1][1]));

  // First URL correlates to one more bitmap.
  ASSERT_EQ(1U, web_app_info.other_icon_bitmaps[kHomeTabIconUrl1].size());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[kHomeTabIconUrl1][0],
                           icons_map[kHomeTabIconUrl1][0]));

  // We end up with one file handler with 6 icon infos. The second URL produces
  // 4 IconInfos because it has two bitmaps and two purposes: 2 x 2 = 4.
  ASSERT_EQ(1U, web_app_info.file_handlers.size());

  // The metadata we expect to be saved after icons are finished downloading and
  // processing. Note that the icon sizes saved to `apps::FileHandler::icons`
  // match downloaded sizes, not those specified in the manifest.
  struct {
    GURL expected_url;
    apps::IconInfo::SquareSizePx expected_size;
    apps::IconInfo::Purpose expected_purpose;
  } expectations[] = {
      {kFileHandlerIconUrl2, 17, apps::IconInfo::Purpose::kAny},
      {kFileHandlerIconUrl2, 29, apps::IconInfo::Purpose::kAny},
      {kFileHandlerIconUrl1, 79, apps::IconInfo::Purpose::kAny},
      {kFileHandlerIconUrl1, 134, apps::IconInfo::Purpose::kAny},
      {kFileHandlerIconUrl1, 79, apps::IconInfo::Purpose::kMaskable},
      {kFileHandlerIconUrl1, 134, apps::IconInfo::Purpose::kMaskable}};

  const size_t num_expectations =
      sizeof(expectations) / sizeof(expectations[0]);
  ASSERT_EQ(num_expectations,
            web_app_info.file_handlers[0].downloaded_icons.size());

  for (size_t i = 0; i < num_expectations; ++i) {
    const auto& icon = web_app_info.file_handlers[0].downloaded_icons[i];
    EXPECT_EQ(expectations[i].expected_url, icon.url);
    EXPECT_EQ(expectations[i].expected_size, icon.square_size_px);
    EXPECT_EQ(expectations[i].expected_purpose, icon.purpose);
  }
}

// Test duplicate icon download urls that from the manifest.
TEST(WebAppInstallUtils, DuplicateIconDownloadURLs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({blink::features::kFileHandlingIcons}, {});

  WebAppInstallInfo web_app_info;

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

  // file handler icons
  {
    apps::FileHandler file_handler;
    std::vector<apps::IconInfo> downloaded_icons;
    {
      apps::IconInfo info;
      info.url = GURL("http://www.chromium.org/image/icon6.png");
      web_app_info.manifest_icons.push_back(info);
    }
    {
      apps::IconInfo info;
      info.url = GURL("http://www.chromium.org/image/icon7.png");
      web_app_info.manifest_icons.push_back(info);
    }
    web_app_info.file_handlers.push_back(file_handler);
  }
  {
    apps::FileHandler file_handler;
    std::vector<apps::IconInfo> downloaded_icons;
    {
      apps::IconInfo info;
      info.url = GURL("http://www.chromium.org/image/icon7.png");
      web_app_info.manifest_icons.push_back(info);
    }
    {
      apps::IconInfo info;
      info.url = GURL("http://www.chromium.org/image/icon8.png");
      web_app_info.manifest_icons.push_back(info);
    }
    web_app_info.file_handlers.push_back(file_handler);
  }

  base::flat_set<GURL> download_urls = GetValidIconUrlsToDownload(web_app_info);

  const size_t download_urls_size = 8;
  EXPECT_EQ(download_urls_size, download_urls.size());
  for (size_t i = 0; i < download_urls_size; i++) {
    std::string url_str = "http://www.chromium.org/image/icon" +
                          base::NumberToString(i + 1) + ".png";
    EXPECT_EQ(1u, download_urls.count(GURL(url_str)));
  }
}

INSTANTIATE_TEST_SUITE_P(, FileHandlersFromManifestTest, testing::Bool());

#if BUILDFLAG(IS_WIN)

// TODO(crbug.com/1403999): Refactor to not using MockOsIntegrationManager once
// removed.
using RegisterOsSettingsTest = testing::Test;

TEST_F(RegisterOsSettingsTest, MaybeRegisterOsUninstall) {
  if (AreSubManagersExecuteEnabled()) {
    GTEST_SKIP() << "Skipping tests as enabling sub managers bypasses "
                    "existing OS integration flow";
  }
  content::BrowserTaskEnvironment task_environment;

  // MaybeRegisterOsUninstall
  // Scenario 1.
  // web app sources: kDefault, kPolicy
  // removed source: kPolicy
  // check web_app.CanUserUninstallWebApp is false
  // check RegisterWebAppOsUninstallation is called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, MacAppShimOnAppInstalledForProfile(app_id)).Times(1);
  EXPECT_CALL(manager, RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(1);
  EXPECT_CALL(manager, Synchronize(app_id, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());

  // Scenario 1.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(WebAppManagement::kDefault);
  web_app->AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());

  base::RunLoop run_loop;
  MaybeRegisterOsUninstall(
      web_app.get(), WebAppManagement::kPolicy, manager,
      base::BindLambdaForTesting(
          [&](OsHooksErrors os_hooks_errors) { run_loop.Quit(); }));
  run_loop.Run();
}

TEST_F(RegisterOsSettingsTest, MaybeRegisterOsSettings_NoRegistration) {
  // MaybeRegisterOsUninstall
  // Scenario 2.
  // web app sources: kSync, kPolicy
  // removed source: kSync
  // check web_app.CanUserUninstallWebApp is false
  // check RegisterWebAppOsUninstallation is not called

  // Scenario 3.
  // web app sources: kDefault, kSync, kWewbAppStore
  // removed source: kSync
  // check web_app.CanUserUninstallWebApp is true
  // check RegisterWebAppOsUninstallation is not called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(0);

  // Scenario 2.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(WebAppManagement::kSync);
  web_app->AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());
  MaybeRegisterOsUninstall(web_app.get(), WebAppManagement::kSync, manager,
                           base::DoNothing());

  // Scenario 3.
  auto web_app2 = std::make_unique<WebApp>(app_id);
  web_app2->AddSource(WebAppManagement::kDefault);
  web_app2->AddSource(WebAppManagement::kSync);
  web_app2->AddSource(WebAppManagement::kWebAppStore);
  EXPECT_TRUE(web_app2->CanUserUninstallWebApp());
  MaybeRegisterOsUninstall(web_app2.get(), WebAppManagement::kDefault, manager,
                           base::DoNothing());
}

TEST_F(RegisterOsSettingsTest, MaybeUnregisterOsUninstall) {
  if (AreSubManagersExecuteEnabled()) {
    GTEST_SKIP() << "Skipping tests as enabling sub managers bypasses "
                    "existing OS integration flow";
  }
  content::BrowserTaskEnvironment task_environment;

  // MaybeUnregisterOsUninstall
  // Scenario 1.
  // web app sources: kDefault
  // added source: kPolicy
  // check web_app.CanUserUninstallWebApp is false
  // check UnregisterWebAppOsUninstallation is called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, Synchronize(app_id, testing::_, testing::_)).Times(1);
  EXPECT_CALL(manager, UnregisterWebAppOsUninstallation(app_id)).Times(1);

  // Scenario 1.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(WebAppManagement::kDefault);
  EXPECT_TRUE(web_app->CanUserUninstallWebApp());
  MaybeUnregisterOsUninstall(web_app.get(), WebAppManagement::kPolicy, manager);
}

TEST_F(RegisterOsSettingsTest, MaybeUnregisterOsSettings_NoUnregistration) {
  // MaybeUnregisterOsUninstall
  // Scenario 2.
  // web app sources: kSync, kPolicy
  // added source: kSync
  // check web_app.CanUserUninstallWebApp is false
  // check UnregisterWebAppOsUninstallation is not called

  // Scenario 3.
  // web app sources: kSync
  // added source: kSync
  // check web_app.CanUserUninstallWebApp is true
  // check UnregisterWebAppOsUninstallation is not called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, UnregisterWebAppOsUninstallation(app_id)).Times(0);

  // Scenario 2.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());
  MaybeUnregisterOsUninstall(web_app.get(), WebAppManagement::kSync, manager);

  // Scenario 3.
  auto web_app2 = std::make_unique<WebApp>(app_id);
  web_app2->AddSource(WebAppManagement::kSync);
  EXPECT_TRUE(web_app2->CanUserUninstallWebApp());
  MaybeUnregisterOsUninstall(web_app2.get(), WebAppManagement::kDefault,
                             manager);
}

#endif  // BUILDFLAG(IS_WIN)

TEST(WebAppInstallUtils, SetWebAppManifestFields_Summary) {
  WebAppInstallInfo web_app_info;
  web_app_info.start_url = GURL("https://www.chromium.org/index.html");
  web_app_info.scope = web_app_info.start_url.GetWithoutFilename();
  web_app_info.title = u"App Name";
  web_app_info.description = u"App Description";
  web_app_info.theme_color = SK_ColorCYAN;
  web_app_info.dark_mode_theme_color = SK_ColorBLACK;
  web_app_info.background_color = SK_ColorMAGENTA;
  web_app_info.dark_mode_background_color = SK_ColorBLACK;

  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, web_app_info.start_url);
  auto web_app = std::make_unique<WebApp>(app_id);
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

  web_app_info.theme_color = absl::nullopt;
  web_app_info.dark_mode_theme_color = absl::nullopt;
  web_app_info.background_color = absl::nullopt;
  web_app_info.dark_mode_background_color = absl::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->theme_color().has_value());
  EXPECT_FALSE(web_app->dark_mode_theme_color().has_value());
  EXPECT_FALSE(web_app->background_color().has_value());
  EXPECT_FALSE(web_app->dark_mode_background_color().has_value());
}

TEST(WebAppInstallUtils, SetWebAppManifestFields_ShareTarget) {
  WebAppInstallInfo web_app_info;
  web_app_info.start_url = StartUrl();
  web_app_info.scope = web_app_info.start_url.GetWithoutFilename();
  web_app_info.title = u"App Name";

  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, web_app_info.start_url);
  auto web_app = std::make_unique<WebApp>(app_id);

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

  web_app_info.share_target = absl::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->share_target().has_value());
}

}  // namespace web_app
