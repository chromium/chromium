// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-data-view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-data-view.h"
#include "third_party/liburlpattern/part.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"
#include "url/origin.h"

namespace web_app {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

namespace {

using testing::ElementsAre;

constexpr SquareSizePx kIconSize = 64;

// This value is greater than `kMaxIcons` in web_app_install_utils.cc.
constexpr unsigned int kNumMaxIcons = 30;
constexpr unsigned int kNumMaxShortcutEntries = 10;

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

// Returns a simple `SafeUrlPattern` for the "foo.com" hostname.
blink::SafeUrlPattern FooUrlPattern() {
  blink::SafeUrlPattern pattern;
  pattern.hostname = {
      liburlpattern::Part(liburlpattern::PartType::kFixed,
                          /*value=*/"foo.com", liburlpattern::Modifier::kNone),
  };
  return pattern;
}

class ManifestToWebAppInstallInfoJobTest : public WebAppTest {
 public:
  ManifestToWebAppInstallInfoJobTest() = default;
  ManifestToWebAppInstallInfoJobTest(
      const ManifestToWebAppInstallInfoJobTest&) = delete;
  ManifestToWebAppInstallInfoJobTest& operator=(
      const ManifestToWebAppInstallInfoJobTest&) = delete;
  ~ManifestToWebAppInstallInfoJobTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
    fake_retriever_ = web_contents_manager().CreateDataRetriever();
  }

  std::unique_ptr<WebAppInstallInfo> GetWebAppInstallInfoFromJob(
      const blink::mojom::Manifest& manifest,
      WebAppInstallInfoConstructOptions construct_options =
          WebAppInstallInfoConstructOptions{}) {
    return test::GetInstallInfoForCurrentManifest(web_contents()->GetWeakPtr(),
                                                  manifest, construct_options);
  }

 protected:
  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  void SetupBasicPageState() {
    auto& page_state = web_contents_manager().GetOrCreatePageState(start_url_);

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = icon_url_;
    icon.sizes = {{kIconSize, kIconSize}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(icon_url_).bitmaps = {
        GetBasicIconBitmap()};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = start_url_;
    manifest->id = GenerateManifestIdFromStartUrlOnly(start_url_);
    manifest->scope = start_url_.GetWithoutFilename();
    manifest->display = DisplayMode::kStandalone;
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->has_background_color = true;
    manifest->background_color = SK_ColorRED;
    manifest->has_theme_color = true;
    manifest->theme_color = SK_ColorGREEN;
    manifest->launch_handler = LaunchHandler(
        blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew);

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(start_url_)
        .manifest_before_default_processing;
  }

  SkBitmap GetBasicIconBitmap() {
    return gfx::test::CreateBitmap(kIconSize, SK_ColorBLUE);
  }

  GURL start_url_{"https://www.foo.bar/index.html"};
  GURL icon_url_{"https://www.foo.bar/icon.png"};
  std::unique_ptr<WebAppDataRetriever> fake_retriever_;
};

TEST_F(ManifestToWebAppInstallInfoJobTest, BasicFieldsPopulated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({blink::features::kFileHandlingIcons,
                                 blink::features::kWebAppManifestLockScreen},
                                /*disabled_features=*/{});

  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  manifest->display_override.push_back(DisplayMode::kMinimalUi);
  manifest->display_override.push_back(DisplayMode::kStandalone);

  manifest->borderless_url_patterns = {FooUrlPattern()};

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
    manifest->file_handlers.push_back(std::move(handler));
  }

  {
    auto protocol_handler = blink::mojom::ManifestProtocolHandler::New();
    protocol_handler->protocol = u"mailto";
    protocol_handler->url = GURL("http://example.com/handle=%s");
    manifest->protocol_handlers.push_back(std::move(protocol_handler));
  }

  {
    auto scope_extension = blink::mojom::ManifestScopeExtension::New();
    scope_extension->origin =
        url::Origin::Create(GURL("https://scope_extensions_origin.com/"));
    scope_extension->has_origin_wildcard = false;
    manifest->scope_extensions.push_back(std::move(scope_extension));
  }

  {
    network::ParsedPermissionsPolicyDeclaration declaration;
    declaration.feature = network::mojom::PermissionsPolicyFeature::kFullscreen;
    declaration.allowed_origins = {
        *network::OriginWithPossibleWildcards::FromOrigin(
            url::Origin::Create(GURL("https://www.example.com")))};
    declaration.matches_all_origins = false;
    declaration.matches_opaque_src = false;

    manifest->permissions_policy.push_back(std::move(declaration));
  }

  {
    blink::Manifest::RelatedApplication related_app;
    related_app.platform = u"platform";
    related_app.url = GURL("http://www.example.com");
    related_app.id = u"id";
    manifest->related_applications.push_back(std::move(related_app));
  }

  {
    auto lock_screen = blink::mojom::ManifestLockScreen::New();
    lock_screen->start_url = GURL("https://www.foo.bar/lock-screen-start-url");
    manifest->lock_screen = std::move(lock_screen);
  }

  {
    // Update with a valid new_note_url.
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url = GURL("https://www.foo.bar/new-note-url");
    manifest->note_taking = std::move(note_taking);
  }

  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_NE(web_app_info, nullptr);

  // Verify basic web app fields populated.
  EXPECT_EQ(u"Foo App", web_app_info->title);
  EXPECT_EQ(DisplayMode::kStandalone, web_app_info->display_mode);
  ASSERT_EQ(2u, web_app_info->display_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info->display_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, web_app_info->display_override[1]);
  EXPECT_EQ(start_url_, web_app_info->start_url());
  EXPECT_EQ(GenerateManifestIdFromStartUrlOnly(start_url_),
            web_app_info->manifest_id());
  EXPECT_EQ(start_url_.GetWithoutFilename(), web_app_info->scope);

  // Verify icon metadata and bitmaps populated correctly.
  EXPECT_EQ(1u, web_app_info->manifest_icons.size());
  EXPECT_EQ(icon_url_.spec(), web_app_info->manifest_icons[0].url);
  EXPECT_TRUE(base::Contains(web_app_info->icon_bitmaps.any, kIconSize));
  EXPECT_THAT(web_app_info->icon_bitmaps.any[kIconSize],
              gfx::test::EqualsBitmap(GetBasicIconBitmap()));

  // Check file handlers were updated.
  ASSERT_EQ(1u, web_app_info->file_handlers.size());
  auto file_handler = web_app_info->file_handlers[0];
  ASSERT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ(file_handler.accept[0].mime_type, "image/png");
  EXPECT_EQ(manifest->file_handlers[0]->action, file_handler.action);
  EXPECT_TRUE(file_handler.accept[0].file_extensions.contains(".png"));

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info->protocol_handlers.size());
  auto protocol_handler = web_app_info->protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, "mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  // Check scope extensions were updated.
  EXPECT_EQ(1u, web_app_info->scope_extensions.size());
  auto scope_extension = *web_app_info->scope_extensions.begin();
  EXPECT_EQ(scope_extension.origin,
            url::Origin::Create(GURL("https://scope_extensions_origin.com/")));
  EXPECT_FALSE(scope_extension.has_origin_wildcard);

  EXPECT_EQ(GURL("https://www.foo.bar/lock-screen-start-url"),
            web_app_info->lock_screen_start_url);

  EXPECT_EQ(GURL("https://www.foo.bar/new-note-url"),
            web_app_info->note_taking_new_note_url);

  // Check permissions policy was updated.
  EXPECT_EQ(1u, web_app_info->permissions_policy.size());
  auto declaration = web_app_info->permissions_policy[0];
  EXPECT_EQ(declaration.feature,
            network::mojom::PermissionsPolicyFeature::kFullscreen);
  EXPECT_EQ(1u, declaration.allowed_origins.size());
  EXPECT_EQ("https://www.example.com",
            declaration.allowed_origins[0].Serialize());
  EXPECT_FALSE(declaration.matches_all_origins);
  EXPECT_FALSE(declaration.matches_opaque_src);

  // Check related applications were updated.
  ASSERT_EQ(1u, web_app_info->related_applications.size());
  auto related_app = web_app_info->related_applications[0];
  EXPECT_EQ(u"platform", related_app.platform);
  EXPECT_EQ(GURL("http://www.example.com"), related_app.url);
  EXPECT_EQ(u"id", related_app.id);

  // Check borderless URL patterns were set.
  EXPECT_THAT(web_app_info->borderless_url_patterns,
              testing::ElementsAre(FooUrlPattern()));
}

TEST_F(ManifestToWebAppInstallInfoJobTest, EmptyNameUsesShortName) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();
  manifest->name = std::nullopt;
  manifest->short_name = u"Shorter name";

  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(u"Shorter name", web_app_info->title);
}

TEST_F(ManifestToWebAppInstallInfoJobTest,
       UpdateIconMeasurementFlagSetMeasuresMemory) {
  base::HistogramTester tester;
  SetupBasicPageState();
  auto web_app_info = GetWebAppInstallInfoFromJob(
      *GetPageManifest(), {.record_icon_results_on_update = true});
  EXPECT_FALSE(web_app_info->icon_bitmaps.empty());
  tester.ExpectTotalCount("WebApp.TotalIconsMemory.DownloadedForUpdate", 1);
}

TEST_F(ManifestToWebAppInstallInfoJobTest, IconParsingCorrectly) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up a maskable and any icon.
  blink::Manifest::ImageResource icon_maskable;
  icon_maskable.src = icon_url_;
  icon_maskable.sizes = {{kIconSize, kIconSize}};
  icon_maskable.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY,
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  web_contents_manager().GetOrCreateIconState(icon_url_).bitmaps = {
      GetBasicIconBitmap()};
  manifest->icons = {icon_maskable};

  // Set up only a monochrome icon.
  GURL monochrome_url("https://www.foo.bar/monochrome.png/");
  SkBitmap monochrome_icon = gfx::test::CreateBitmap(kIconSize, SK_ColorGREEN);
  int monochrome_size = 64;
  blink::Manifest::ImageResource icon_monochrome;
  icon_monochrome.src = monochrome_url;
  icon_monochrome.sizes = {{monochrome_size, monochrome_size}};
  icon_monochrome.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MONOCHROME};
  web_contents_manager().GetOrCreateIconState(monochrome_url).bitmaps = {
      monochrome_icon};
  manifest->icons.push_back(icon_monochrome);

  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);

  // Verify icon metadata populated correctly (first 2 are any and maskable
  // icons, the last one is monochrome).
  EXPECT_EQ(3u, web_app_info->manifest_icons.size());
  EXPECT_EQ(icon_url_.spec(), web_app_info->manifest_icons[0].url);
  EXPECT_EQ(icon_url_.spec(), web_app_info->manifest_icons[1].url);
  EXPECT_EQ(monochrome_url.spec(), web_app_info->manifest_icons[2].url);
  std::map<IconPurpose, int> purpose_to_count;
  for (const auto& icon_info : web_app_info->manifest_icons) {
    purpose_to_count[IconInfoPurposeToManifestPurpose(icon_info.purpose)]++;
  }
  EXPECT_EQ(1, purpose_to_count[IconPurpose::ANY]);
  EXPECT_EQ(1, purpose_to_count[IconPurpose::MONOCHROME]);
  EXPECT_EQ(1, purpose_to_count[IconPurpose::MASKABLE]);

  // Verify icon bitmaps populated correctly.
  EXPECT_TRUE(base::Contains(web_app_info->icon_bitmaps.maskable, kIconSize));
  EXPECT_THAT(web_app_info->icon_bitmaps.maskable[kIconSize],
              gfx::test::EqualsBitmap(GetBasicIconBitmap()));
  EXPECT_TRUE(base::Contains(web_app_info->icon_bitmaps.any, kIconSize));
  EXPECT_THAT(web_app_info->icon_bitmaps.any[kIconSize],
              gfx::test::EqualsBitmap(GetBasicIconBitmap()));
  EXPECT_TRUE(
      base::Contains(web_app_info->icon_bitmaps.monochrome, monochrome_size));
  EXPECT_THAT(web_app_info->icon_bitmaps.monochrome[monochrome_size],
              gfx::test::EqualsBitmap(monochrome_icon));
}

TEST_F(ManifestToWebAppInstallInfoJobTest, ShareTarget) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();
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
    manifest->share_target = std::move(share_target);
  }

  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_TRUE(web_app_info->share_target.has_value());
  const auto& share_target = *web_app_info->share_target;
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

  // Reset the share target field.
  manifest->share_target = std::nullopt;
  auto web_app_info2 = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_FALSE(web_app_info2->share_target.has_value());
}

TEST_F(ManifestToWebAppInstallInfoJobTest, ShortcutItems) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up shortcut icon metadata in the manifest.
  const GURL shortcut_url("http://www.foo.bar/shortcuts/action");
  const std::u16string shortcut_name(u"shortcut_1");
  blink::Manifest::ShortcutItem shortcut_item;
  shortcut_item.name = std::u16string(shortcut_name);
  shortcut_item.url = shortcut_url;

  blink::Manifest::ImageResource icon;
  const GURL icon_url("http://www.foo.bar/shortcuts/icon.png");
  int shortcut_icon_size = 16;
  icon.src = icon_url;
  icon.sizes.emplace_back(shortcut_icon_size, shortcut_icon_size);
  icon.purpose = {Purpose::ANY};
  shortcut_item.icons.push_back(icon);
  const SkBitmap shortcut_bitmap =
      gfx::test::CreateBitmap(shortcut_icon_size, SK_ColorRED);
  manifest->shortcuts.push_back(shortcut_item);
  web_contents_manager().GetOrCreateIconState(icon_url).bitmaps = {
      shortcut_bitmap};

  // Verify shortcut menu item infos are populated correctly.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(1u, web_app_info->shortcuts_menu_item_infos.size());
  EXPECT_EQ(1u, web_app_info->shortcuts_menu_item_infos[0]
                    .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                    .size());
  WebAppShortcutsMenuItemInfo::Icon web_app_shortcut_icon =
      web_app_info->shortcuts_menu_item_infos[0].GetShortcutIconInfosForPurpose(
          IconPurpose::ANY)[0];
  EXPECT_EQ(icon_url, web_app_shortcut_icon.url);

  EXPECT_FALSE(web_app_info->shortcuts_menu_icon_bitmaps.empty());
  EXPECT_TRUE(base::Contains(web_app_info->shortcuts_menu_icon_bitmaps[0].any,
                             shortcut_icon_size));
  EXPECT_THAT(
      web_app_info->shortcuts_menu_icon_bitmaps[0].any[shortcut_icon_size],
      gfx::test::EqualsBitmap(shortcut_bitmap));
}

TEST_F(ManifestToWebAppInstallInfoJobTest, Translations) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  {
    blink::Manifest::TranslationItem item;
    item.name = "name 1";
    item.short_name = "short name 1";
    item.description = "description 1";

    manifest->translations[u"language 1"] = std::move(item);
  }
  {
    blink::Manifest::TranslationItem item;
    item.short_name = "short name 2";
    item.description = "description 2";

    manifest->translations[u"language 2"] = std::move(item);
  }
  {
    blink::Manifest::TranslationItem item;
    item.name = "name 3";

    manifest->translations[u"language 3"] = std::move(item);
  }

  // Verify translation items are populated correctly.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(3u, web_app_info->translations.size());
  EXPECT_EQ(web_app_info->translations["language 1"].name, "name 1");
  EXPECT_EQ(web_app_info->translations["language 1"].short_name,
            "short name 1");
  EXPECT_EQ(web_app_info->translations["language 1"].description,
            "description 1");

  EXPECT_FALSE(web_app_info->translations["language 2"].name);
  EXPECT_EQ(web_app_info->translations["language 2"].short_name,
            "short name 2");
  EXPECT_EQ(web_app_info->translations["language 2"].description,
            "description 2");

  EXPECT_EQ(web_app_info->translations["language 3"].name, "name 3");
  EXPECT_FALSE(web_app_info->translations["language 3"].short_name);
  EXPECT_FALSE(web_app_info->translations["language 3"].description);
}

TEST_F(ManifestToWebAppInstallInfoJobTest, TabStripMetadata) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  blink::Manifest::ImageResource icon;
  const GURL app_icon("fav1.png");
  icon.purpose = {Purpose::ANY};
  icon.src = app_icon;

  TabStrip tab_strip;
  blink::Manifest::HomeTabParams home_tab_params;
  home_tab_params.icons.push_back(icon);
  tab_strip.home_tab = home_tab_params;

  blink::Manifest::NewTabButtonParams new_tab_button_params;
  new_tab_button_params.url = GURL("https://www.example.com/");
  tab_strip.new_tab_button = new_tab_button_params;
  manifest->tab_strip = std::move(tab_strip);

  // Verify tab strip items are populated correctly.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);

  EXPECT_TRUE(web_app_info->tab_strip.has_value());
  EXPECT_EQ(std::get<blink::Manifest::HomeTabParams>(
                web_app_info->tab_strip.value().home_tab)
                .icons.size(),
            1u);
  EXPECT_EQ(std::get<blink::Manifest::HomeTabParams>(
                web_app_info->tab_strip.value().home_tab)
                .icons[0]
                .src,
            app_icon);
  EXPECT_EQ(web_app_info->tab_strip.value().new_tab_button.url,
            GURL("https://www.example.com/"));
}

TEST_F(ManifestToWebAppInstallInfoJobTest, HomeTabAndFileHandlingIcons) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kFileHandlingIcons);
  WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(true);
  SetupBasicPageState();
  auto& manifest = GetPageManifest();
  GURL tab_strip_icon_url("http://www.foo.bar/tab_strip/icon.png");
  int tab_strip_size = 16;
  GURL file_handler_icon_url("http://www.foo.bar/file_handler/icon.png");
  int file_handler_size = 32;

  // Set up tabstrip metadata with icons.
  TabStrip tab_strip;
  tab_strip.home_tab = web_app::TabStrip::Visibility::kAuto;
  blink::Manifest::HomeTabParams home_tab_params;
  blink::Manifest::ImageResource icon;
  icon.src = tab_strip_icon_url;
  icon.purpose.push_back(web_app::Purpose::ANY);
  icon.sizes.emplace_back(tab_strip_size, tab_strip_size);
  home_tab_params.icons.push_back(std::move(icon));

  tab_strip.home_tab = home_tab_params;
  manifest->tab_strip = std::move(tab_strip);

  // Set up file handler metadata with icons.
  {
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://www.foo.bar/open-files");
    handler->accept[u"image/png"].push_back(u".png");
    handler->name = u"Images";
    {
      blink::Manifest::ImageResource file_icon;
      file_icon.src = file_handler_icon_url;
      file_icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      file_icon.sizes.emplace_back(file_handler_size, file_handler_size);
      handler->icons.push_back(file_icon);
    }
    manifest->file_handlers.push_back(std::move(handler));
  }

  // Ensure icons are set up correctly in the web_contents.
  SkBitmap tab_strip_icon =
      gfx::test::CreateBitmap(tab_strip_size, SK_ColorBLUE);
  web_contents_manager().GetOrCreateIconState(tab_strip_icon_url).bitmaps = {
      tab_strip_icon};
  SkBitmap file_handler_icon =
      gfx::test::CreateBitmap(file_handler_size, SK_ColorRED);
  web_contents_manager().GetOrCreateIconState(file_handler_icon_url).bitmaps = {
      file_handler_icon};

  // Verify bitmaps are populated correctly.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(2u, web_app_info->other_icon_bitmaps.size());
  EXPECT_TRUE(
      base::Contains(web_app_info->other_icon_bitmaps, tab_strip_icon_url));
  EXPECT_TRUE(
      base::Contains(web_app_info->other_icon_bitmaps, file_handler_icon_url));
  EXPECT_THAT(web_app_info->other_icon_bitmaps[tab_strip_icon_url][0],
              gfx::test::EqualsBitmap(tab_strip_icon));
  EXPECT_THAT(web_app_info->other_icon_bitmaps[file_handler_icon_url][0],
              gfx::test::EqualsBitmap(file_handler_icon));
}

TEST_F(ManifestToWebAppInstallInfoJobTest, TabIconsLargeSizeIgnored) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  TabStrip tab_strip;
  tab_strip.home_tab = web_app::TabStrip::Visibility::kAuto;
  blink::Manifest::HomeTabParams home_tab_params;
  GURL tab_strip_icon_url("http://www.foo.bar/tab_strip/icon.png");
  for (int size = 1023; size <= 1026; ++size) {
    blink::Manifest::ImageResource icon;
    icon.src = tab_strip_icon_url;
    icon.purpose.push_back(web_app::Purpose::ANY);
    icon.sizes.emplace_back(size, size);
    home_tab_params.icons.push_back(std::move(icon));
  }

  tab_strip.home_tab = home_tab_params;
  manifest->tab_strip = std::move(tab_strip);

  // Ensure the icon of size 1024 is available in the web contents.
  web_contents_manager().GetOrCreateIconState(tab_strip_icon_url).bitmaps = {
      GetBasicIconBitmap()};

  // Verify tab strip items are populated correctly.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_TRUE(web_app_info->tab_strip.has_value());
  const auto& home_tab = std::get<blink::Manifest::HomeTabParams>(
      web_app_info->tab_strip.value().home_tab);
  EXPECT_EQ(2U, home_tab.icons.size());

  EXPECT_EQ(1u, web_app_info->other_icon_bitmaps.size());
}

// Tests that we limit the number of shortcut menu items.
TEST_F(ManifestToWebAppInstallInfoJobTest, TooManyShortcuts) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  for (unsigned int i = 1; i <= kNumMaxShortcutEntries + 1; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = u"Name" + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.foo.bar/shortcuts/action");
    manifest->shortcuts.push_back(shortcut_item);
  }
  EXPECT_LT(kNumMaxShortcutEntries, manifest->shortcuts.size());
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(kNumMaxShortcutEntries,
            web_app_info->shortcuts_menu_item_infos.size());
}

// Tests that we limit the number of icons in the manifest to 20 as per
// `kMaxIcons`.
TEST_F(ManifestToWebAppInstallInfoJobTest, TooManyIcons) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  for (unsigned int i = 1; i <= kNumMaxIcons + 1; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("icon.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(i, i);
    manifest->icons.push_back(std::move(icon));
  }
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(20u, web_app_info->manifest_icons.size());
}

// Tests that we limit the size of icons declared by a site.
TEST_F(ManifestToWebAppInstallInfoJobTest, LargeIconsDiscardedForNormalIcons) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  for (int size = 1023; size <= 1026; ++size) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("icon.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(size, size);
    manifest->icons.push_back(std::move(icon));
  }
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  for (const apps::IconInfo& icon : web_app_info->manifest_icons) {
    EXPECT_LE(icon.square_size_px, 1024);
  }
}

// Tests that we limit the size of shortcut icons declared by a site.
TEST_F(ManifestToWebAppInstallInfoJobTest, LargeIconsDiscardedForShortcut) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  for (int size = 1023; size <= 1026; ++size) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = u"Shortcut" + base::NumberToString16(size);
    shortcut_item.url = GURL("http://www.foo.bar/shortcuts/action");

    blink::Manifest::ImageResource icon;
    icon.src = GURL("icon.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(size, size);
    shortcut_item.icons.push_back(std::move(icon));

    manifest->shortcuts.push_back(shortcut_item);
  }
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  // Only 2 icons are within the size limit
  std::vector<WebAppShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info->shortcuts_menu_item_infos) {
    for (const auto& icon_info :
         shortcut.GetShortcutIconInfosForPurpose(IconPurpose::ANY)) {
      all_icons.push_back(icon_info);
    }
  }
  // Only the early icons are within the size limit.
  EXPECT_EQ(2U, all_icons.size());
}

TEST_F(ManifestToWebAppInstallInfoJobTest, CrossOriginUrls_DropFields) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kWebAppManifestLockScreen);
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  {
    auto lock_screen = blink::mojom::ManifestLockScreen::New();
    lock_screen->start_url =
        GURL("http://www.some-other-origin.com/lock-screen-start-url");
    manifest->lock_screen = std::move(lock_screen);
  }

  {
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url =
        GURL("http://www.some-other-origin.com/new-note-url");
    manifest->note_taking = std::move(note_taking);
  }
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_TRUE(web_app_info->lock_screen_start_url.is_empty());
  EXPECT_TRUE(web_app_info->note_taking_new_note_url.is_empty());
}

TEST_F(ManifestToWebAppInstallInfoJobTest, InvalidManifestUrl) {
  SetupBasicPageState();
  auto& manifest = GetPageManifest();
  manifest->manifest_url = GURL("foo");

  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_TRUE(web_app_info->manifest_url.is_empty());
}

// Tests proper parsing of ManifestImageResource icons from the manifest into
// |icons_with_size_any| based on the absence of a size parameter.
TEST_F(ManifestToWebAppInstallInfoJobTest,
       PopulateAnyIconsCorrectlyManifestParsingSVGOnly) {
  WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(/*value=*/true);
  base::test::ScopedFeatureList feature_list(
      blink::features::kFileHandlingIcons);
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Generate expected data structure for |icons_with_size_any|.
  IconsWithSizeAny expected_icon_metadata;

  const GURL manifest_icon_no_size_url(
      "https://www.example.com/manifest_image_no_size.svg");
  const GURL manifest_icon_size_url(
      "https://www.example.com/manifest_image_size.svg");
  const GURL file_handling_no_size_url(
      "https://www.example.com/file_handling_no_size.svg");
  const GURL file_handling_size_url(
      "https://www.example.com/file_handling_size.png");
  const GURL shortcut_icon_no_size_url(
      "https://www.example.com/shortcut_menu_icon_no_size.svg");
  const GURL shortcut_icon_size_url(
      "https://www.example.com/shortcut_menu_icon_size.svg");
  const GURL tab_strip_icon_no_size_url(
      "https://www.example.com/tab_strip_icon_no_size.svg");
  const GURL tab_strip_icon_size_url(
      "https://www.example.com/tab_strip_icon_size.jpg");

  // Sample manifest icons, one with a size specified, one without.
  blink::Manifest::ImageResource manifest_icon_no_size;
  manifest_icon_no_size.src = manifest_icon_no_size_url;
  manifest_icon_no_size.sizes = {{0, 0}, {196, 196}};
  manifest_icon_no_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY,
      blink::mojom::ManifestImageResource_Purpose::MONOCHROME};
  manifest->icons.push_back(std::move(manifest_icon_no_size));

  // Set up the expected icon metadata for manifest icons.
  expected_icon_metadata.manifest_icons[IconPurpose::ANY] =
      manifest_icon_no_size_url;
  expected_icon_metadata.manifest_icons[IconPurpose::MONOCHROME] =
      manifest_icon_no_size_url;
  expected_icon_metadata.manifest_icon_provided_sizes.emplace(196, 196);

  blink::Manifest::ImageResource manifest_icon_size;
  manifest_icon_size.src = manifest_icon_size_url;
  manifest_icon_size.sizes = {{24, 24}};
  manifest_icon_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons.push_back(std::move(manifest_icon_size));
  expected_icon_metadata.manifest_icon_provided_sizes.emplace(24, 24);

  // Sample file handler with no size specified for icons.
  auto file_handler = blink::mojom::ManifestFileHandler::New();
  file_handler->action = GURL("https://www.action.com/");
  file_handler->name = u"Random File";
  file_handler->accept[u"text/html"] = {u".html"};

  blink::Manifest::ImageResource file_handling_icon_no_size;
  file_handling_icon_no_size.src = file_handling_no_size_url;
  file_handling_icon_no_size.sizes = {{0, 0}};
  file_handling_icon_no_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  file_handler->icons.push_back(std::move(file_handling_icon_no_size));

  // Set up the expected icon metadata for file handling icons.
  expected_icon_metadata.file_handling_icons[IconPurpose::MASKABLE] =
      file_handling_no_size_url;

  blink::Manifest::ImageResource file_handling_icon_size;
  file_handling_icon_size.src = file_handling_size_url;
  file_handling_icon_size.sizes = {{64, 64}};
  file_handling_icon_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MONOCHROME};
  file_handler->icons.push_back(std::move(file_handling_icon_size));
  manifest->file_handlers.push_back(std::move(file_handler));
  expected_icon_metadata.file_handling_icon_provided_sizes.emplace(64, 64);

  // Sample shortcut menu item info with no size specified for icons.
  blink::Manifest::ShortcutItem shortcut_item;
  shortcut_item.name = u"Shortcut Name";
  shortcut_item.url = GURL("https://www.example.com");

  blink::Manifest::ImageResource shortcut_icon_no_size;
  shortcut_icon_no_size.src = shortcut_icon_no_size_url;
  shortcut_icon_no_size.sizes = {{0, 0}, {512, 512}};
  shortcut_icon_no_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY};
  shortcut_item.icons.push_back(std::move(shortcut_icon_no_size));

  // Set up the expected icon metadata for shortcut menu icons.
  expected_icon_metadata.shortcut_menu_icons[IconPurpose::ANY] =
      shortcut_icon_no_size_url;
  expected_icon_metadata.shortcut_menu_icons_provided_sizes.emplace(512, 512);

  blink::Manifest::ImageResource shortcut_icon_with_size;
  shortcut_icon_with_size.src = shortcut_icon_size_url;
  shortcut_icon_with_size.sizes = {{48, 48}};
  shortcut_icon_with_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  shortcut_item.icons.push_back(std::move(shortcut_icon_with_size));
  manifest->shortcuts.push_back(std::move(shortcut_item));
  expected_icon_metadata.shortcut_menu_icons_provided_sizes.emplace(48, 48);

  // Sample home tab strip metadata with no size specified for icons.
  TabStrip tab_strip;
  blink::Manifest::HomeTabParams home_tab_params;

  blink::Manifest::ImageResource tab_strip_icon_no_size;
  tab_strip_icon_no_size.src = tab_strip_icon_no_size_url;
  tab_strip_icon_no_size.sizes = {{0, 0}};
  tab_strip_icon_no_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MONOCHROME};
  home_tab_params.icons.push_back(std::move(tab_strip_icon_no_size));

  blink::Manifest::ImageResource tab_strip_icon_size;
  tab_strip_icon_size.src = tab_strip_icon_size_url;
  tab_strip_icon_size.sizes = {{16, 16}};
  tab_strip_icon_size.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY};
  home_tab_params.icons.push_back(std::move(tab_strip_icon_size));
  tab_strip.home_tab = std::move(home_tab_params);
  manifest->tab_strip = std::move(tab_strip);

  // Set up the expected icon metadata for home tab icons.
  expected_icon_metadata.home_tab_icons[IconPurpose::MONOCHROME] =
      tab_strip_icon_no_size_url;
  expected_icon_metadata.home_tab_icon_provided_sizes.emplace(16, 16);

  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  ASSERT_EQ(expected_icon_metadata, web_app_info->icons_with_size_any);
}

TEST_F(ManifestToWebAppInstallInfoJobTest, DeferIconFetching) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kFileHandlingIcons);
  WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(true);

  // This manifest already has a default icon of size 64 populated that is blue
  // in color.
  SetupBasicPageState();
  blink::mojom::ManifestPtr& manifest = GetPageManifest();

  // Set up shortcut menu item metadata with icons in the manifest and the web
  // contents.
  GURL shortcut_icon_url("http://www.foo.bar/shortcut_icon/icon.png");
  int shortcut_size = 32;
  blink::Manifest::ImageResource shortcut_icon;
  shortcut_icon.src = shortcut_icon_url;
  shortcut_icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  shortcut_icon.sizes.emplace_back(shortcut_size, shortcut_size);
  blink::Manifest::ShortcutItem shortcut;
  shortcut.name = u"Shortcut";
  shortcut.url = GURL("http://www.foo.bar/shortcut/");
  shortcut.icons = {shortcut_icon};
  manifest->shortcuts.push_back(shortcut);

  SkBitmap shortcut_icon_bitmap =
      gfx::test::CreateBitmap(shortcut_size, SK_ColorRED);
  auto& shortcut_icon_state =
      web_contents_manager().GetOrCreateIconState(shortcut_icon_url);
  shortcut_icon_state.bitmaps = {shortcut_icon_bitmap};

  // Verify no icons are downloaded.
  base::Value::Dict debug_data;
  base::test::TestFuture<std::unique_ptr<WebAppInstallInfo>> future;
  std::unique_ptr<WebAppDataRetriever> retriever =
      provider().web_contents_manager().CreateDataRetriever();
  WebAppInstallInfoConstructOptions options;
  options.defer_icon_fetching = true;
  auto job = ManifestToWebAppInstallInfoJob::CreateAndStart(
      *manifest, *retriever.get(), /*background_installation=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(), [](IconUrlSizeSet&) {}, debug_data,
      future.GetCallback(), options);
  ASSERT_TRUE(future.Wait(base::RunLoop::Type::kNestableTasksAllowed));

  std::unique_ptr<WebAppInstallInfo> web_app_info = future.Take();
  EXPECT_TRUE(web_app_info->icon_bitmaps.empty());
  EXPECT_TRUE(web_app_info->trusted_icon_bitmaps.empty());
  EXPECT_TRUE(web_app_info->other_icon_bitmaps.empty());

  // Verify that the product icon is fetched, but the shortcut icon isn't.
  bool product_icon_fetched = false;
  bool shortcut_icon_fetched = false;
  shortcut_icon_state.on_icon_fetched =
      base::BindLambdaForTesting([&]() { shortcut_icon_fetched = true; });
  web_contents_manager().GetOrCreateIconState(icon_url_).on_icon_fetched =
      base::BindLambdaForTesting([&]() { product_icon_fetched = true; });

  base::test::TestFuture<void> icons_fetched;
  job->FetchIcons(*web_app_info, *web_contents(), icons_fetched.GetCallback(),
                  /*icon_url_modifications=*/std::nullopt,
                  {.shortcut_menu_item_icons = false});
  ASSERT_TRUE(icons_fetched.Wait(base::RunLoop::Type::kNestableTasksAllowed));

  EXPECT_TRUE(product_icon_fetched);
  EXPECT_FALSE(shortcut_icon_fetched);

  ASSERT_TRUE(!web_app_info->trusted_icons.empty());
  EXPECT_THAT(web_app_info->trusted_icon_bitmaps.any[kIconSize],
              gfx::test::EqualsBitmap(GetBasicIconBitmap()));
  ASSERT_TRUE(!web_app_info->icon_bitmaps.empty());
  EXPECT_THAT(web_app_info->icon_bitmaps.any[kIconSize],
              gfx::test::EqualsBitmap(GetBasicIconBitmap()));
}

// Unit-tests verifying the trusted icon behavior.
class ManifestToWebAppInstallInfoTrustedIconTest
    : public ManifestToWebAppInstallInfoJobTest {
 protected:
  bool ShouldPreferMaskable() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
    return true;
#else
    return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  }

  base::test::ScopedFeatureList feature_list_{features::kWebAppUsePrimaryIcon};
};

TEST_F(ManifestToWebAppInstallInfoTrustedIconTest, ChooseLargestIconAny) {
  const GURL larger_icon_url{"https://www.foo.bar/icon_larger.png"};
  const int larger_icon_size = 128;
  const SkBitmap larger_icon =
      gfx::test::CreateBitmap(larger_icon_size, SK_ColorGREEN);
  const GURL largest_icon_url{"https://www.foo.bar/icon_largest.png"};
  const int largest_icon_size = 512;

  const SkBitmap largest_icon =
      gfx::test::CreateBitmap(largest_icon_size, SK_ColorBLUE);

  // The manifest already has an icon of size 64x64 set.
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up 2 more icons, and verify that the largest icon gets loaded for
  // purpose ANY.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = larger_icon_url;
  icon_larger.sizes = {{larger_icon_size, larger_icon_size}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons.push_back(std::move(icon_larger));
  web_contents_manager().GetOrCreateIconState(larger_icon_url).bitmaps = {
      larger_icon};

  blink::Manifest::ImageResource icon_largest;
  icon_largest.src = largest_icon_url;
  icon_largest.sizes = {{largest_icon_size, largest_icon_size}};
  icon_largest.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons.push_back(std::move(icon_largest));
  web_contents_manager().GetOrCreateIconState(largest_icon_url).bitmaps = {
      largest_icon};

  // Verify the largest icon is chosen to be used as the trusted icon.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  ASSERT_EQ(web_app_info->trusted_icons.size(), 1u);
  apps::IconInfo trusted_info(largest_icon_url, largest_icon_size);
  trusted_info.purpose = apps::IconInfo::Purpose::kAny;
  EXPECT_THAT(web_app_info->trusted_icons, ElementsAre(trusted_info));

  // Verify that manifest_icons are populated in the order they are specified in
  // the manifest.
  ASSERT_EQ(web_app_info->manifest_icons.size(), 3u);

  apps::IconInfo info1(icon_url_, kIconSize);
  info1.purpose = apps::IconInfo::Purpose::kAny;
  apps::IconInfo info2(larger_icon_url, larger_icon_size);
  info2.purpose = apps::IconInfo::Purpose::kAny;
  apps::IconInfo info3(largest_icon_url, largest_icon_size);
  info3.purpose = apps::IconInfo::Purpose::kAny;
  EXPECT_THAT(web_app_info->manifest_icons, ElementsAre(info1, info2, info3));

  EXPECT_FALSE(web_app_info->trusted_icon_bitmaps.any.empty());
  EXPECT_TRUE(web_app_info->trusted_icon_bitmaps.maskable.empty());

  // Verify expected bitmap chosen of proper size.
  EXPECT_THAT(web_app_info->trusted_icon_bitmaps.any[512],
              gfx::test::EqualsBitmap(largest_icon));

  // Verify bitmaps populated properly for `any` icons.
  for (const auto& icon_data : web_app_info->trusted_icon_bitmaps.any) {
    const SkBitmap& bitmap = icon_data.second;
    gfx::test::CheckColors(
        bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2), SK_ColorBLUE);
  }
}

TEST_F(ManifestToWebAppInstallInfoTrustedIconTest,
       MaskableIconNotChosenIfBelow256) {
  const GURL larger_icon_url{"https://www.foo.bar/icon_larger.png"};
  const int larger_icon_size = 96;
  const SkBitmap larger_icon =
      gfx::test::CreateBitmap(larger_icon_size, SK_ColorGREEN);

  const GURL largest_icon_maskable_url{"https://www.foo.bar/icon_largest.png"};
  const int largest_icon_size = 128;
  const SkBitmap largest_icon_maskable =
      gfx::test::CreateBitmap(largest_icon_size, SK_ColorBLUE);

  // The manifest already has an icon of size 64x64 set.
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up 2 more icons, and verify that the larger icon gets loaded for
  // purpose ANY.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = larger_icon_url;
  icon_larger.sizes = {{larger_icon_size, larger_icon_size}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons.push_back(std::move(icon_larger));
  web_contents_manager().GetOrCreateIconState(larger_icon_url).bitmaps = {
      larger_icon};

  blink::Manifest::ImageResource icon_largest;
  icon_largest.src = largest_icon_maskable_url;
  icon_largest.sizes = {{largest_icon_size, largest_icon_size}};
  icon_largest.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  manifest->icons.push_back(std::move(icon_largest));
  web_contents_manager()
      .GetOrCreateIconState(largest_icon_maskable_url)
      .bitmaps = {largest_icon_maskable};

  // Verify the icon of purpose 'Any' is chosen to be used as the trusted icon.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  ASSERT_EQ(web_app_info->manifest_icons.size(), 3u);
  ASSERT_EQ(web_app_info->trusted_icons.size(), 1u);
  apps::IconInfo trusted_info(larger_icon_url, larger_icon_size);
  trusted_info.purpose = apps::IconInfo::Purpose::kAny;
  EXPECT_THAT(web_app_info->trusted_icons, ElementsAre(trusted_info));

  // Verify that the larger icon is chosen an all platforms, even though there
  // is a larger maskable icon, because it's size is not greater than 256.
  EXPECT_TRUE(web_app_info->trusted_icon_bitmaps.maskable.empty());
  EXPECT_THAT(web_app_info->trusted_icon_bitmaps.any[96],
              gfx::test::EqualsBitmap(larger_icon));
}

// Choose same icon, to be used as `maskable` or `any` depending on whichever OS
// it is used on.
TEST_F(ManifestToWebAppInstallInfoTrustedIconTest, MultiPurposeIcons) {
  const GURL larger_icon_url{"https://www.foo.bar/icon_larger.png"};
  const int larger_icon_size = 128;
  const SkBitmap larger_icon =
      gfx::test::CreateBitmap(larger_icon_size, SK_ColorGREEN);
  const GURL largest_icon_url{"https://www.foo.bar/icon_largest.png"};
  const int largest_icon_size = 512;
  const SkBitmap largest_icon =
      gfx::test::CreateBitmap(largest_icon_size, SK_ColorBLUE);

  // The manifest already has an icon of size 64x64 set.
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up 2 more icons, and verify that the largest icon gets loaded for
  // purpose ANY.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = larger_icon_url;
  icon_larger.sizes = {{larger_icon_size, larger_icon_size}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons.push_back(std::move(icon_larger));
  web_contents_manager().GetOrCreateIconState(larger_icon_url).bitmaps = {
      larger_icon};

  blink::Manifest::ImageResource icon_largest;
  icon_largest.src = largest_icon_url;
  icon_largest.sizes = {{largest_icon_size, largest_icon_size}};
  icon_largest.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY,
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  manifest->icons.push_back(std::move(icon_largest));
  web_contents_manager().GetOrCreateIconState(largest_icon_url).bitmaps = {
      largest_icon};

  // Verify the icon of purpose 'maskable` is chosen as the trusted icon on Mac
  // and ChromeOS, while on Linux and Windows, `any` is chosen as the trusted
  // icon
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  ASSERT_EQ(web_app_info->manifest_icons.size(), 4u);

  ASSERT_EQ(web_app_info->trusted_icons.size(), 1u);
  apps::IconInfo trusted_info(largest_icon_url, largest_icon_size);
  trusted_info.purpose = ShouldPreferMaskable()
                             ? apps::IconInfo::Purpose::kMaskable
                             : apps::IconInfo::Purpose::kAny;
  EXPECT_THAT(web_app_info->trusted_icons, ElementsAre(trusted_info));

  ASSERT_EQ(web_app_info->trusted_icon_bitmaps.any.size() > 0,
            !ShouldPreferMaskable());
  ASSERT_EQ(web_app_info->trusted_icon_bitmaps.maskable.size() > 0,
            ShouldPreferMaskable());

  // Verify the same bitmap has been parsed and generated as per the
  // requirements, since it is both maskable and any, and is the largest icon.
  const std::map<SquareSizePx, SkBitmap>& icon_map_parsed =
      ShouldPreferMaskable() ? web_app_info->trusted_icon_bitmaps.maskable
                             : web_app_info->trusted_icon_bitmaps.any;
  for (const auto& icon_data : icon_map_parsed) {
    const SkBitmap& bitmap = icon_data.second;
    gfx::test::CheckColors(
        bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2), SK_ColorBLUE);
  }
}

TEST_F(ManifestToWebAppInstallInfoTrustedIconTest,
       AnyAndMaskableAsPerOsBitmaps) {
  const GURL larger_icon_any_url{"https://www.foo.bar/icon_larger.png"};
  const int larger_icon_size = 128;
  const SkColor icon_color_any = SK_ColorGREEN;
  const SkBitmap larger_icon_any =
      gfx::test::CreateBitmap(larger_icon_size, icon_color_any);

  const GURL largest_icon_maskable_url{"https://www.foo.bar/icon_largest.png"};
  const int largest_icon_size = 256;
  const SkColor icon_color_maskable = SK_ColorBLUE;
  const SkBitmap largest_icon_maskable =
      gfx::test::CreateBitmap(largest_icon_size, icon_color_maskable);

  // The manifest already has an icon of size 64x64 set.
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up 2 more icons.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = larger_icon_any_url;
  icon_larger.sizes = {{larger_icon_size, larger_icon_size}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons.push_back(std::move(icon_larger));
  web_contents_manager().GetOrCreateIconState(larger_icon_any_url).bitmaps = {
      larger_icon_any};

  blink::Manifest::ImageResource icon_largest;
  icon_largest.src = largest_icon_maskable_url;
  icon_largest.sizes = {{largest_icon_size, largest_icon_size}};
  icon_largest.purpose = {
      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  manifest->icons.push_back(std::move(icon_largest));
  web_contents_manager()
      .GetOrCreateIconState(largest_icon_maskable_url)
      .bitmaps = {largest_icon_maskable};

  // Verify the icon of purpose 'maskable` is chosen as the trusted icon on Mac
  // and ChromeOS, while on Linux and Windows, `any` is chosen as the trusted
  // icon
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  ASSERT_EQ(web_app_info->manifest_icons.size(), 3u);

  ASSERT_EQ(web_app_info->trusted_icon_bitmaps.any.size() > 0,
            !ShouldPreferMaskable());
  ASSERT_EQ(web_app_info->trusted_icon_bitmaps.maskable.size() > 0,
            ShouldPreferMaskable());

  // Verify the correct bitmap has been parsed and generated as per the
  // requirements.
  const std::map<SquareSizePx, SkBitmap>& icon_map_parsed =
      ShouldPreferMaskable() ? web_app_info->trusted_icon_bitmaps.maskable
                             : web_app_info->trusted_icon_bitmaps.any;
  const SkColor final_color =
      ShouldPreferMaskable() ? icon_color_maskable : icon_color_any;
  for (const auto& icon_data : icon_map_parsed) {
    const SkBitmap& bitmap = icon_data.second;
    gfx::test::CheckColors(
        bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2), final_color);
  }
}

TEST_F(ManifestToWebAppInstallInfoTrustedIconTest,
       NotPopulatedIfGeneratedIcons) {
  const GURL larger_icon_url{"https://www.foo.bar/icon_larger.png"};
  const int larger_icon_size = 128;
  const SkBitmap larger_icon =
      gfx::test::CreateBitmap(larger_icon_size, SK_ColorGREEN);

  // The manifest already has an icon of size 64x64 set.
  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up icon, but don't add it to the WebContentsManager, to mimic icon
  // downloading failure.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = larger_icon_url;
  icon_larger.sizes = {{larger_icon_size, larger_icon_size}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  manifest->icons = {icon_larger};

  // Verify that the `web_app_info` has generated icons, but no trusted icons
  // metadata.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(web_app_info->manifest_icons.size(), 1u);
  EXPECT_TRUE(web_app_info->is_generated_icon);
  EXPECT_FALSE(web_app_info->trusted_icons.empty());
  EXPECT_TRUE(web_app_info->trusted_icon_bitmaps.any.empty());
}

TEST_F(ManifestToWebAppInstallInfoTrustedIconTest, SVGIconsNoSize) {
  const GURL svg_icon_url{"https://www.foo.bar/icon_larger.svg"};
  const SkBitmap expected_svg_icon =
      gfx::test::CreateBitmap(1024, SK_ColorGREEN);

  // The manifest already has an icon of size 64x64 set. Clear that.
  SetupBasicPageState();
  auto& manifest = GetPageManifest();
  manifest->icons.clear();

  // Set an SVG icon of size (0,0), regardless of purpose.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = svg_icon_url;
  icon_larger.sizes = {{0, 0}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY,
                         blink::mojom::ManifestImageResource_Purpose::MASKABLE};
  manifest->icons.push_back(std::move(icon_larger));

  web_contents_manager().GetOrCreateIconState(svg_icon_url).bitmaps = {
      expected_svg_icon};

  // Verify that the `web_app_info` has the correct icon metadata for the SVG
  // icon.
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest);
  EXPECT_EQ(web_app_info->manifest_icons.size(), 2u);
  ASSERT_EQ(web_app_info->trusted_icons.size(), 1u);
  apps::IconInfo trusted_info(svg_icon_url, /*size=*/1024);
  trusted_info.purpose = ShouldPreferMaskable()
                             ? apps::IconInfo::Purpose::kMaskable
                             : apps::IconInfo::Purpose::kAny;
  EXPECT_THAT(web_app_info->trusted_icons, ElementsAre(trusted_info));

  ASSERT_EQ(web_app_info->trusted_icon_bitmaps.any.size() > 0,
            !ShouldPreferMaskable());
  ASSERT_EQ(web_app_info->trusted_icon_bitmaps.maskable.size() > 0,
            ShouldPreferMaskable());

  // Verify the correct bitmap has been parsed and downloaded as per the
  // requirements.
  const std::map<SquareSizePx, SkBitmap>& icon_map_parsed =
      ShouldPreferMaskable() ? web_app_info->trusted_icon_bitmaps.maskable
                             : web_app_info->trusted_icon_bitmaps.any;
  for (const auto& icon_data : icon_map_parsed) {
    const SkBitmap& bitmap = icon_data.second;
    gfx::test::CheckColors(
        bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2),
        SK_ColorGREEN);
  }
}

// Verifies that when `use_manifest_icons_as_trusted` is set, the manifest icons
// are overwritten as trusted icons. The `ChooseLargestIconAny` test verifies
// the default behavior.
TEST_F(ManifestToWebAppInstallInfoTrustedIconTest,
       ConfigureManifestIconsAsTrusted) {
  const GURL larger_icon_url{"https://www.foo.bar/icon_larger.png"};
  const int larger_icon_size = 128;
  const SkBitmap larger_icon =
      gfx::test::CreateBitmap(larger_icon_size, SK_ColorGREEN);
  const GURL largest_icon_url{"https://www.foo.bar/icon_largest.png"};
  const int largest_icon_size = 512;
  const SkBitmap largest_icon =
      gfx::test::CreateBitmap(largest_icon_size, SK_ColorBLUE);

  SetupBasicPageState();
  auto& manifest = GetPageManifest();

  // Set up 2 more icons, and verify that the largest icon gets loaded for
  // purpose ANY.
  blink::Manifest::ImageResource icon_larger;
  icon_larger.src = larger_icon_url;
  icon_larger.sizes = {{larger_icon_size, larger_icon_size}};
  icon_larger.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  web_contents_manager().GetOrCreateIconState(larger_icon_url).bitmaps = {
      larger_icon};

  blink::Manifest::ImageResource icon_largest;
  icon_largest.src = largest_icon_url;
  icon_largest.sizes = {{largest_icon_size, largest_icon_size}};
  icon_largest.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  web_contents_manager().GetOrCreateIconState(largest_icon_url).bitmaps = {
      largest_icon};
  manifest->icons = {icon_larger, icon_largest};

  // Get the trusted icons from the manifest with the flag set accordingly.
  WebAppInstallInfoConstructOptions options;
  options.use_manifest_icons_as_trusted = true;
  auto web_app_info = GetWebAppInstallInfoFromJob(*manifest, options);

  apps::IconInfo info1(larger_icon_url, larger_icon_size);
  info1.purpose = apps::IconInfo::Purpose::kAny;
  apps::IconInfo info2(largest_icon_url, largest_icon_size);
  info2.purpose = apps::IconInfo::Purpose::kAny;

  // Verify that the trusted icon and the manifest icon bitmaps are the same.
  EXPECT_THAT(web_app_info->trusted_icons, ElementsAre(info1, info2));
  EXPECT_THAT(web_app_info->manifest_icons, ElementsAre(info1, info2));

  // Verify expected bitmap chosen of proper size.
  EXPECT_FALSE(web_app_info->trusted_icon_bitmaps.any.empty());
  EXPECT_TRUE(web_app_info->trusted_icon_bitmaps.maskable.empty());

  // Verify bitmaps populated properly for `any` icons, matching manifest icons.
  EXPECT_THAT(web_app_info->trusted_icon_bitmaps.any[larger_icon_size],
              gfx::test::EqualsBitmap(larger_icon));
  EXPECT_THAT(web_app_info->trusted_icon_bitmaps.any[largest_icon_size],
              gfx::test::EqualsBitmap(largest_icon));
}

}  // namespace

}  // namespace web_app
