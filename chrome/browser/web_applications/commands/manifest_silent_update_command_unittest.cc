// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {
class ManifestSilentUpdateCommandTest : public WebAppTest {
 public:
  ManifestSilentUpdateCommandTest() = default;
  ManifestSilentUpdateCommandTest(const ManifestSilentUpdateCommandTest&) =
      delete;
  ManifestSilentUpdateCommandTest& operator=(
      const ManifestSilentUpdateCommandTest&) = delete;
  ~ManifestSilentUpdateCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetOriginAssociationManager(
        std::make_unique<FakeWebAppOriginAssociationManager>());
    provider->StartWithSubsystems();
    test::WaitUntilReady(provider);

    web_contents_manager().SetUrlLoaded(web_contents(), app_url());
  }

 protected:
  void SetupBasicInstallablePageState() {
    auto& page_state = web_contents_manager().GetOrCreatePageState(app_url());

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = default_icon_url_;
    icon.sizes = {{manifest_icon_size_, manifest_icon_size_}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(default_icon_url_).bitmaps = {
        gfx::test::CreateBitmap(manifest_icon_size_, manifest_icon_color_)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = app_url();
    manifest->id = GenerateManifestIdFromStartUrlOnly(app_url());
    manifest->scope = app_url().GetWithoutFilename();
    manifest->display = DisplayMode::kStandalone;
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->has_background_color = true;
    manifest->background_color = manifest_icon_color_;
    manifest->has_theme_color = true;
    manifest->theme_color = manifest_icon_color_;
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url = GURL("https://www.foo.bar/web_apps/new_note");
    manifest->note_taking = std::move(note_taking);
    manifest->launch_handler = LaunchHandler(
        blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew);

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  ManifestSilentUpdateCheckResult RunManifestUpdateAndGetResult() {
    base::test::TestFuture<ManifestSilentUpdateCheckResult>
        manifest_silent_update_future;
    fake_provider().scheduler().ScheduleManifestSilentUpdate(
        app_url(), web_contents()->GetWeakPtr(),
        manifest_silent_update_future.GetCallback());

    EXPECT_TRUE(manifest_silent_update_future.Wait());
    return manifest_silent_update_future.Get();
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(app_url())
        .manifest_before_default_processing;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  GURL app_url() { return app_url_; }
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

 private:
  const GURL app_url_{"https://www.foo.bar/web_apps/basic.html"};
  const GURL default_icon_url_{"https://example.com/path/def_icon.png"};
  const SkColor manifest_icon_color_ = SK_ColorCYAN;
  const int manifest_icon_size_ = 96;
};

TEST_F(ManifestSilentUpdateCommandTest, VerifyAppUpToDate) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kAppUpToDate,
                              /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, AppNotInstalledNotSilentlyUpdated) {
  SetupBasicInstallablePageState();

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppNotInstalled);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kAppNotInstalled,
                              /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, StartUrlUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& new_manifest = GetPageManifest();
  const GURL new_start_url("https://www.foo.bar/new_scope/new_basic.html");
  new_manifest->start_url = new_start_url;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            new_start_url);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, ThemeColorUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorCYAN);

  auto& new_manifest = GetPageManifest();
  new_manifest->theme_color = SK_ColorYELLOW;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorYELLOW);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, BackgroundColorUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppBackgroundColor(app_id),
            SK_ColorCYAN);

  auto& new_manifest = GetPageManifest();
  new_manifest->background_color = SK_ColorYELLOW;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppBackgroundColor(app_id),
            SK_ColorYELLOW);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, DisplayModeUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(
      provider().registrar_unsafe().GetEffectiveDisplayModeFromManifest(app_id),
      DisplayMode::kStandalone);

  auto& new_manifest = GetPageManifest();
  new_manifest->display = DisplayMode::kBrowser;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(
      provider().registrar_unsafe().GetEffectiveDisplayModeFromManifest(app_id),
      DisplayMode::kBrowser);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, ScopeUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(app_id),
            app_url().GetWithoutFilename());

  auto& new_manifest = GetPageManifest();
  const GURL new_scope("https://www.foo.bar/new_scope/");
  new_manifest->scope = new_scope;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(app_id), new_scope);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, DisplayOverrideUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppDisplayModeOverride(app_id).empty());
  EXPECT_EQ(provider().registrar_unsafe().GetAppEffectiveDisplayMode(app_id),
            DisplayMode::kStandalone);

  auto& new_manifest = GetPageManifest();
  std::vector<DisplayMode> new_display_override = {DisplayMode::kMinimalUi,
                                                   DisplayMode::kBrowser};
  new_manifest->display_override = new_display_override;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppDisplayModeOverride(app_id),
            new_display_override);
  EXPECT_EQ(provider().registrar_unsafe().GetAppEffectiveDisplayMode(app_id),
            DisplayMode::kMinimalUi);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, NoteTakingUrlUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppById(app_id)
                ->note_taking_new_note_url(),
            GURL("https://www.foo.bar/web_apps/new_note"));

  auto& new_manifest = GetPageManifest();
  const GURL new_note_url("https://www.foo.bar/web_apps/new_note_new");
  new_manifest->note_taking->new_note_url = new_note_url;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppById(app_id)
                ->note_taking_new_note_url(),
            new_note_url);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, ShortcutsMenuItemInfosUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppShortcutsMenuItemInfos(app_id)
                  .empty());

  auto& new_manifest = GetPageManifest();

  {
    blink::Manifest::ShortcutItem shortcut;
    shortcut.name = u"New Shortcut";
    shortcut.url = GURL("https://www.foo.bar/new_shortcut");
    new_manifest->shortcuts.push_back(std::move(shortcut));
  }

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  const auto& new_shortcuts =
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(app_id);
  EXPECT_THAT(new_shortcuts,
              testing::ElementsAre(testing::AllOf(
                  testing::Field(&web_app::WebAppShortcutsMenuItemInfo::name,
                                 u"New Shortcut"),
                  testing::Field(&web_app::WebAppShortcutsMenuItemInfo::url,
                                 GURL("https://www.foo.bar/new_shortcut")))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, ShareTargetUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_FALSE(provider().registrar_unsafe().GetAppShareTarget(app_id));

  auto& new_manifest = GetPageManifest();

  {
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("https://www.foo.bar/share");
    share_target.method = blink::mojom::ManifestShareTarget_Method::kPost;
    share_target.enctype =
        blink::mojom::ManifestShareTarget_Enctype::kFormUrlEncoded;
    blink::Manifest::ShareTargetParams params;
    share_target.params = std::move(params);
    new_manifest->share_target.emplace(std::move(share_target));
  }

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppShareTarget(app_id)->action,
            GURL("https://www.foo.bar/share"));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, ProtocolHandlersUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(app_id)
                  ->protocol_handlers()
                  .empty());

  auto& new_manifest = GetPageManifest();

  {
    auto protocol_handler = blink::mojom::ManifestProtocolHandler::New();
    protocol_handler->protocol = u"mailto";
    protocol_handler->url = GURL("http://example.com/handle=%s");
    new_manifest->protocol_handlers.push_back(std::move(protocol_handler));
  }

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  const auto& new_protocol_handlers =
      provider().registrar_unsafe().GetAppById(app_id)->protocol_handlers();
  EXPECT_THAT(
      new_protocol_handlers,
      testing::ElementsAre(testing::AllOf(
          testing::Field(&apps::ProtocolHandlerInfo::protocol, "mailto"),
          testing::Field(&apps::ProtocolHandlerInfo::url,
                         GURL("http://example.com/handle=%s")))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, FileHandlersUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppFileHandlers(app_id)->empty());

  auto& new_manifest = GetPageManifest();

  {
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://example.com/open-files");
    handler->accept[u"image/png"].push_back(u".png");
    handler->name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = GURL("fav1.png");
      icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY,
                      blink::mojom::ManifestImageResource_Purpose::MASKABLE};
      handler->icons.push_back(icon);
    }
    new_manifest->file_handlers.push_back(std::move(handler));
  }

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  const auto* new_file_handlers =
      provider().registrar_unsafe().GetAppFileHandlers(app_id);
  EXPECT_THAT(
      *new_file_handlers,
      testing::ElementsAre(testing::AllOf(
          testing::Field(&apps::FileHandler::action,
                         GURL("http://example.com/open-files")),
          testing::Field(&apps::FileHandler::display_name, u"Images"))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, LaunchHandlerUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppById(app_id)->launch_handler(),
            LaunchHandler(
                blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew));

  auto& new_manifest = GetPageManifest();
  new_manifest->launch_handler = LaunchHandler(
      blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting);

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->launch_handler(),
      LaunchHandler(
          blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, ScopeExtensionsUpdatedSilently) {
  scoped_feature_list_.InitWithFeatures(
      {blink::features::kWebAppEnableScopeExtensions}, {});
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_TRUE(provider().registrar_unsafe().GetScopeExtensions(app_id).empty());

  auto& new_manifest = GetPageManifest();

  {
    auto scope_extension = blink::mojom::ManifestScopeExtension::New();
    scope_extension->origin =
        url::Origin::Create(GURL("https://scope_extensions_new.com/"));
    scope_extension->has_origin_wildcard = false;
    new_manifest->scope_extensions.push_back(std::move(scope_extension));
  }

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  const auto& new_scope_extensions =
      provider().registrar_unsafe().GetScopeExtensions(app_id);
  EXPECT_THAT(
      new_scope_extensions,
      testing::ElementsAre(testing::AllOf(
          testing::Field(
              &web_app::ScopeExtensionInfo::origin,
              url::Origin::Create(GURL("https://scope_extensions_new.com/"))),
          testing::Field(&web_app::ScopeExtensionInfo::has_origin_wildcard,
                         false))));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, TabStripUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_FALSE(provider().registrar_unsafe().GetAppById(app_id)->tab_strip());

  auto& new_manifest = GetPageManifest();
  {
    TabStrip tab_strip;
    tab_strip.home_tab = TabStrip::Visibility::kAuto;
    tab_strip.new_tab_button.url = GURL("https://www.random.com/");
    new_manifest->tab_strip = std::move(tab_strip);
  }

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(app_id)
                  ->tab_strip()
                  .has_value());
  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppById(app_id)
                ->tab_strip()
                ->new_tab_button.url->spec(),
            "https://www.random.com/");
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

// TODO(crbug.com/424246884): Check for lock_screen_start_url to update if the
// feature is enabled.

}  // namespace web_app
