// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-data-view.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {

class ManifestSilentUpdateCommandTest : public WebAppTest {
 public:
  const GURL kDefaultIconUrl = GURL("https://example.com/path/def_icon.png");
  const GURL kAppUrl = GURL("https://www.foo.bar/web_apps/basic.html");
  static constexpr SkColor kManifestIconColor = SK_ColorCYAN;
  static constexpr int kManifestIconSize = 96;

  ManifestSilentUpdateCommandTest() = default;
  ManifestSilentUpdateCommandTest(const ManifestSilentUpdateCommandTest&) =
      delete;
  ManifestSilentUpdateCommandTest& operator=(
      const ManifestSilentUpdateCommandTest&) = delete;
  ~ManifestSilentUpdateCommandTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kWebAppUsePrimaryIcon,
         features::kSilentPolicyAndDefaultAppUpdating},
        {});
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetOriginAssociationManager(
        std::make_unique<FakeWebAppOriginAssociationManager>());
    provider->StartWithSubsystems();
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider);

    web_contents_manager().SetUrlLoaded(web_contents(), kAppUrl);
  }

 protected:
  void SetupBasicInstallablePageState() {
    auto& page_state = web_contents_manager().GetOrCreatePageState(kAppUrl);

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = kDefaultIconUrl;
    icon.sizes = {{kManifestIconSize, kManifestIconSize}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(kDefaultIconUrl).bitmaps = {
        gfx::test::CreateBitmap(kManifestIconSize, kManifestIconColor)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kAppUrl);
    manifest->scope = kAppUrl.GetWithoutFilename();
    manifest->display = DisplayMode::kStandalone;
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->has_background_color = true;
    manifest->background_color = kManifestIconColor;
    manifest->has_theme_color = true;
    manifest->theme_color = kManifestIconColor;
    manifest->has_valid_specified_start_url = true;
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url = GURL("https://www.foo.bar/web_apps/new_note");
    manifest->note_taking = std::move(note_taking);
    manifest->launch_handler = LaunchHandler(
        blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew);

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  ManifestSilentUpdateCheckResult RunManifestUpdateAndGetResult(
      std::optional<base::Time> previous_time_for_silent_icon_update =
          std::nullopt) {
    base::test::TestFuture<ManifestSilentUpdateCompletionInfo>
        manifest_silent_update_future;
    fake_provider().scheduler().ScheduleManifestSilentUpdate(
        *web_contents(), previous_time_for_silent_icon_update,
        manifest_silent_update_future.GetCallback());

    EXPECT_TRUE(manifest_silent_update_future.Wait());
    return manifest_silent_update_future.Take().result;
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(kAppUrl)
        .manifest_before_default_processing;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }

  bool AppHasPendingUpdateInfo(const webapps::AppId& app_id) {
    return provider()
        .registrar_unsafe()
        .GetAppById(app_id)
        ->pending_update_info()
        .has_value();
  }

  base::FilePath GetAppPendingTrustedIconsDir(Profile* profile,
                                              const webapps::AppId& app_id) {
    base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
    base::FilePath app_dir =
        GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
    return app_dir.AppendASCII("Pending Trusted Icons");
  }

  base::FilePath GetAppPendingManifestIconsDir(Profile* profile,
                                               const webapps::AppId& app_id) {
    base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
    base::FilePath app_dir =
        GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
    return app_dir.AppendASCII("Pending Manifest Icons");
  }

  std::vector<int> GetStoredIconSizesForPurpose(
      const google::protobuf::RepeatedPtrField<proto::DownloadedIconSizeInfo>&
          downloaded_icons,
      sync_pb::WebAppIconInfo_Purpose purpose) {
    for (const auto& info : downloaded_icons) {
      if (info.purpose() == purpose) {
        const auto& sizes_field = info.icon_sizes();
        return std::vector<int>(sizes_field.begin(), sizes_field.end());
      }
    }
    return {};
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
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
            ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate,
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, EmptyStartUpdated) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& page_state = web_contents_manager().GetOrCreatePageState(kAppUrl);
  page_state.error_code =
      webapps::InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR;

  // The document url matches the start_url, so it should resolve just fine.
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kInvalidManifest);
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kInvalidManifest,
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
            kAppUrl.GetWithoutFilename());

  auto& new_manifest = GetPageManifest();
  const GURL new_scope("https://www.foo.bar/new_scope/");
  new_manifest->scope = new_scope;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

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
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

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

TEST_F(ManifestSilentUpdateCommandTest, AppNameChangedPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  std::optional<proto::PendingUpdateInfo> pending_update_info =
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info();
  ASSERT_TRUE(AppHasPendingUpdateInfo(app_id));
  EXPECT_TRUE(pending_update_info->has_name());
  EXPECT_EQ(pending_update_info->name(), base::UTF16ToUTF8(u"New Name"));

  // New pending updates always come with a clean slate, and needs to show up on
  // the UX.
  EXPECT_TRUE(pending_update_info->has_was_ignored());
  EXPECT_FALSE(pending_update_info->was_ignored());
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       AppNameAndStartUrlChangedPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"Foo App"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";
  const GURL new_start_url("https://www.foo.bar/new_scope/new_basic.html");
  new_manifest->start_url = new_start_url;

  EXPECT_EQ(
      RunManifestUpdateAndGetResult(),
      ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges);

  std::optional<proto::PendingUpdateInfo> pending_update_info =
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info();
  ASSERT_TRUE(AppHasPendingUpdateInfo(app_id));
  EXPECT_TRUE(pending_update_info->has_name());
  EXPECT_EQ(pending_update_info->name(), base::UTF16ToUTF8(u"New Name"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            new_start_url);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(
          ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges,
          /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       IconLessThanTenPercentChangedDiffUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};

  SkBitmap changed_bitmap = gfx::test::CreateBitmap(96, SK_ColorCYAN);
  // For a 96x96 image, total pixels = 9216.
  // 10% of 9216 = 921.6 pixels.
  // We'll change a small area, for example, the first 9 rows, to a different
  // color. 9 rows * 96 columns = 864 pixels changed. This is < 10%.
  changed_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorRED);

  // Set icon in content.
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {changed_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  // Verify pending update icon bitmaps are not saved to disk.
  EXPECT_FALSE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_FALSE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       IconLessThanTenPercentDiffThrottledTwiceResult) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));

  // Setup bitmap icons so that bitmap1 and bitmap2 are the same, with only
  // color differences in pixels, and both being <10% diff from each other. This
  // makes it easier to trigger multiple icon updates back to back.
  // For a 96x96 image, total pixels = 9216.
  // 10% of 9216 = 921.6 pixels.
  // We'll change a small area, for example, the first 9 rows, to a different
  // color. 9 rows * 96 columns = 864 pixels changed. This is < 10%.
  GURL bitmap_url1 = GURL("https://example2.com/path/def_icon.png");
  SkBitmap bitmap1 = gfx::test::CreateBitmap(96, SK_ColorCYAN);
  bitmap1.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorRED);

  GURL bitmap_url2 = GURL("https://example3.com/path/def_icon.png");
  SkBitmap bitmap2 = gfx::test::CreateBitmap(96, SK_ColorCYAN);
  bitmap1.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorWHITE);

  // Trigger first update, verify that app icon updates are applied silently.
  auto& new_manifest = GetPageManifest();
  blink::Manifest::ImageResource first_update_icon;
  first_update_icon.src = bitmap_url1;
  first_update_icon.sizes = {{96, 96}};
  first_update_icon.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY};
  new_manifest->icons = {first_update_icon};
  web_contents_manager().GetOrCreateIconState(bitmap_url1).bitmaps = {bitmap1};
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            bitmap_url1);

  // Trigger another update directly after the first update, but this time, set
  // up the manifest to point to the 2nd bitmap.
  blink::Manifest::ImageResource second_update_icon;
  second_update_icon.src = bitmap_url2;
  second_update_icon.sizes = {{96, 96}};
  second_update_icon.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY};
  new_manifest->icons = {second_update_icon};
  web_contents_manager().GetOrCreateIconState(bitmap_url2).bitmaps = {bitmap2};
  EXPECT_EQ(
      RunManifestUpdateAndGetResult(base::Time::Now()),
      ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle);

  // Pending update should be stored on the disk, but the app icons shouldn't be
  // updated.
  ASSERT_TRUE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            bitmap_url1);

  // Verify pending update icon bitmaps are saved to disk.
  EXPECT_TRUE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_TRUE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(
          base::Bucket(ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                       /*count=*/1),
          base::Bucket(ManifestSilentUpdateCheckResult::
                           kAppHasSecurityUpdateDueToThrottle,
                       /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       IconLessThanTenPercentAndStartUrlChangedDiffUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};
  new_manifest->start_url = GURL("https://www.foo.bar/web_apps/new_basic.html");

  SkBitmap changed_bitmap = gfx::test::CreateBitmap(96, SK_ColorCYAN);
  // For a 96x96 image, total pixels = 9216.
  // 10% of 9216 = 921.6 pixels.
  // We'll change a small area, for example, the first 9 rows, to a different
  // color. 9 rows * 96 columns = 864 pixels changed. This is < 10%.
  changed_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorRED);

  // Set icon in content.
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {changed_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/new_basic.html");

  // Verify pending update icon bitmaps are not saved to disk.
  EXPECT_FALSE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_FALSE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       IconMoreThanTenPercentDiffChangedPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{30, 30}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};

  // Set icon in content. Setting the icon color to YELLOW and changing the icon
  // size to trigger a more than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(30, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  std::optional<proto::PendingUpdateInfo> pending_update_info =
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info();
  ASSERT_TRUE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(pending_update_info->manifest_icons_size(), 1);
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->url(),
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->purpose(),
            sync_pb::WebAppIconInfo_Purpose_ANY);
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->size_in_px(), 30);

  EXPECT_EQ(pending_update_info->trusted_icons_size(), 1);
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->url(),
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->purpose(),
            sync_pb::WebAppIconInfo_Purpose_ANY);
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->size_in_px(), 30);

  EXPECT_TRUE(pending_update_info->has_was_ignored());
  EXPECT_FALSE(pending_update_info->was_ignored());

  // There are 6 sizes that will be generated as per SizesToGenerate() in the
  // web app icon manager. With the new icon of size 30, that will be 7
  // downloaded icon sizes in total.
  EXPECT_THAT(GetStoredIconSizesForPurpose(
                  pending_update_info->downloaded_manifest_icons(),
                  sync_pb::WebAppIconInfo_Purpose_ANY),
              testing::UnorderedElementsAre(30, 32, 48, 64, 96, 128, 256));
  EXPECT_THAT(GetStoredIconSizesForPurpose(
                  pending_update_info->downloaded_trusted_icons(),
                  sync_pb::WebAppIconInfo_Purpose_ANY),
              testing::UnorderedElementsAre(30, 32, 48, 64, 96, 128, 256));

  // Verify pending update icon bitmaps are written to disk.
  EXPECT_TRUE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_TRUE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));
  SkBitmap disk_bitmap = web_app::test::LoadTestImageFromDisk(
                             GetAppPendingTrustedIconsDir(profile(), app_id)
                                 .Append(FILE_PATH_LITERAL("Icons/30.png")),
                             /*read_from_test_dir=*/false)
                             .AsBitmap();
  EXPECT_THAT(disk_bitmap, gfx::test::EqualsBitmap(updated_bitmap));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       IconMoreThanTenPercentDiffAndNameChangedPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};
  new_manifest->name = u"New Name";

  // Set icon in content. Setting the icon color to YELLOW to trigger a more
  // than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  std::optional<proto::PendingUpdateInfo> pending_update_info =
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info();
  ASSERT_TRUE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(pending_update_info->manifest_icons_size(), 1);
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->url(),
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->purpose(),
            sync_pb::WebAppIconInfo_Purpose_ANY);
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->size_in_px(), 96);

  EXPECT_EQ(pending_update_info->trusted_icons_size(), 1);
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->url(),
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->purpose(),
            sync_pb::WebAppIconInfo_Purpose_ANY);
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->size_in_px(), 96);

  EXPECT_TRUE(pending_update_info->has_was_ignored());
  EXPECT_FALSE(pending_update_info->was_ignored());

  // Verify pending update icon bitmaps are written to disk.
  EXPECT_TRUE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_TRUE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));
  SkBitmap disk_bitmap = web_app::test::LoadTestImageFromDisk(
                             GetAppPendingTrustedIconsDir(profile(), app_id)
                                 .Append(FILE_PATH_LITERAL("Icons/96.png")),
                             /*read_from_test_dir=*/false)
                             .AsBitmap();
  EXPECT_THAT(disk_bitmap, gfx::test::EqualsBitmap(updated_bitmap));

  EXPECT_EQ(pending_update_info->name(), base::UTF16ToUTF8(u"New Name"));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate,
                  /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       IconMoreThanTenPercentDiffAndStartUrlChangedPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};
  new_manifest->start_url = GURL("https://www.foo.bar/web_apps/new_basic.html");
  // Set icon in content. Setting the icon color to YELLOW to trigger a more
  // than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(
      RunManifestUpdateAndGetResult(),
      ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges);

  std::optional<proto::PendingUpdateInfo> pending_update_info =
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info();
  ASSERT_TRUE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(pending_update_info->manifest_icons_size(), 1);
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->url(),
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->purpose(),
            sync_pb::WebAppIconInfo_Purpose_ANY);
  EXPECT_EQ(pending_update_info->manifest_icons().begin()->size_in_px(), 96);

  EXPECT_EQ(pending_update_info->trusted_icons_size(), 1);
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->url(),
            GURL("https://example2.com/path/def_icon.png"));
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->purpose(),
            sync_pb::WebAppIconInfo_Purpose_ANY);
  EXPECT_EQ(pending_update_info->trusted_icons().begin()->size_in_px(), 96);

  EXPECT_TRUE(pending_update_info->has_was_ignored());
  EXPECT_FALSE(pending_update_info->was_ignored());

  // Verify pending update icon bitmaps are written to disk.
  EXPECT_TRUE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_TRUE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));
  SkBitmap disk_bitmap = web_app::test::LoadTestImageFromDisk(
                             GetAppPendingTrustedIconsDir(profile(), app_id)
                                 .Append(FILE_PATH_LITERAL("Icons/96.png")),
                             /*read_from_test_dir=*/false)
                             .AsBitmap();
  EXPECT_THAT(disk_bitmap, gfx::test::EqualsBitmap(updated_bitmap));

  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/new_basic.html");
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(
          ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges,
          /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, VerifyNoManifestIconsAppUpToDate) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  auto icon_infos = provider().registrar_unsafe().GetAppIconInfos(app_id);
  ASSERT_EQ(icon_infos.size(), 1u);
  EXPECT_EQ(icon_infos.front().url,
            GURL("https://example.com/path/def_icon.png"));

  auto& new_manifest = GetPageManifest();
  new_manifest->icons = {};
  // Note: The InstallableParams options should cause this error to return if
  // there are no icons, but the testing dependency faking isn't quite set up
  // for this yet to happen automatically. So set this manually on the fake.
  web_contents_manager().GetOrCreatePageState(kAppUrl).error_code =
      webapps::InstallableStatusCode::NO_ICON_AVAILABLE;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kInvalidManifest);

  // No icon changes.
  icon_infos = provider().registrar_unsafe().GetAppIconInfos(app_id);
  ASSERT_EQ(icon_infos.size(), 1u);
  EXPECT_EQ(icon_infos.front().url,
            GURL("https://example.com/path/def_icon.png"));

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kInvalidManifest,
                              /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       NoManifestIconsAndStartUrlChangedAppUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& new_manifest = GetPageManifest();
  new_manifest->icons = {};
  // Note: The InstallableParams options should cause this error to return if
  // there are no icons, but the testing dependency faking isn't quite set up
  // for this yet to happen automatically. So set this manually on the fake.
  web_contents_manager().GetOrCreatePageState(kAppUrl).error_code =
      webapps::InstallableStatusCode::NO_ICON_AVAILABLE;
  const GURL new_start_url("https://www.foo.bar/new_scope/new_basic.html");
  new_manifest->start_url = new_start_url;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kInvalidManifest);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kInvalidManifest,
                              /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest,
       NoManifestIconsAndAppNameChangedNoUpdate) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();
  new_manifest->icons = {};
  // Note: The InstallableParams options should cause this error to return if
  // there are no icons, but the testing dependency faking isn't quite set up
  // for this yet to happen automatically. So set this manually on the fake.
  web_contents_manager().GetOrCreatePageState(kAppUrl).error_code =
      webapps::InstallableStatusCode::NO_ICON_AVAILABLE;
  new_manifest->name = u"New Name";

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kInvalidManifest);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kInvalidManifest,
                              /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, NoIconToIcons) {
  // Install via WebAppInstallInfo, with no icons and OS integration, to full
  // icons.
  auto web_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
  web_app_install_info->title = u"A Basic Web App";
  web_app_install_info->display_mode = DisplayMode::kStandalone;
  web_app_install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_install_info->is_generated_icon = true;
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info));

  SetupBasicInstallablePageState();
  ManifestSilentUpdateCheckResult result = RunManifestUpdateAndGetResult();
  EXPECT_TRUE(IsAppUpdated(result));
}

TEST_F(ManifestSilentUpdateCommandTest, OpenInBrowserTabNoUpdate) {
  auto web_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
  web_app_install_info->title = u"A Basic Web App";
  web_app_install_info->display_mode = DisplayMode::kBrowser;
  web_app_install_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info));

  SetupBasicInstallablePageState();
  ManifestSilentUpdateCheckResult result = RunManifestUpdateAndGetResult();
  EXPECT_FALSE(IsAppUpdated(result));
}

TEST_F(ManifestSilentUpdateCommandTest, IconDownloadFailure) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  // First update.
  blink::Manifest::ImageResource second_icon;
  second_icon.src = GURL("https://example2.com/path/def_icon.png");
  second_icon.sizes = {{96, 96}};
  second_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  GetPageManifest()->icons = {second_icon};
  SkBitmap second_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError);
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));
}

TEST_F(ManifestSilentUpdateCommandTest, UpdateTwiceSameManifest) {
  // Have the silent update command triggered twice for the same 'new' manifest.
  // The icon shouldn't be downloaded the second time, as it should be already
  // stored in pending_update_info.

  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  GetPageManifest()->icons = {new_icon};

  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  auto& new_icon_state = web_contents_manager().GetOrCreateIconState(
      GURL("https://example2.com/path/def_icon.png"));
  new_icon_state.bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  // Run a second time, and make sure that the icon is not fetched again.
  bool product_icon_fetched = false;
  new_icon_state.on_icon_fetched =
      base::BindLambdaForTesting([&]() { product_icon_fetched = true; });

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);
  EXPECT_FALSE(product_icon_fetched);
  EXPECT_TRUE(AppHasPendingUpdateInfo(app_id));
}

TEST_F(ManifestSilentUpdateCommandTest, UpdateTwiceNewManifest) {
  // Have the silent update command triggered twice with different icons each
  // time. The resulting pending update info should be from the second one.

  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  // First update.
  blink::Manifest::ImageResource second_icon;
  second_icon.src = GURL("https://example2.com/path/def_icon.png");
  second_icon.sizes = {{96, 96}};
  second_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  GetPageManifest()->icons = {second_icon};
  SkBitmap second_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  auto& second_icon_state = web_contents_manager().GetOrCreateIconState(
      GURL("https://example2.com/path/def_icon.png"));
  second_icon_state.bitmaps = {second_bitmap};
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  // Second update.
  // Verify the icon is fetched.
  blink::Manifest::ImageResource third_icon;
  third_icon.src = GURL("https://example3.com/path/def_icon.png");
  third_icon.sizes = {{96, 96}};
  third_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  GetPageManifest()->icons = {third_icon};
  SkBitmap third_bitmap = gfx::test::CreateBitmap(96, SK_ColorRED);
  auto& third_icon_state = web_contents_manager().GetOrCreateIconState(
      GURL("https://example3.com/path/def_icon.png"));
  third_icon_state.bitmaps = {third_bitmap};
  bool product_icon_fetched = false;
  third_icon_state.on_icon_fetched =
      base::BindLambdaForTesting([&]() { product_icon_fetched = true; });
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);
  EXPECT_TRUE(product_icon_fetched);
  EXPECT_TRUE(AppHasPendingUpdateInfo(app_id));
}

TEST_F(ManifestSilentUpdateCommandTest, UpdateThenBackToOriginal) {
  // Have the silent update command triggered twice, where the second reverts
  // back to the original state, which should clear the pending update info.

  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  // First update.
  blink::Manifest::ImageResource second_icon;
  second_icon.src = GURL("https://example2.com/path/def_icon.png");
  second_icon.sizes = {{96, 96}};
  second_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  GetPageManifest()->icons = {second_icon};
  SkBitmap second_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  auto& second_icon_state = web_contents_manager().GetOrCreateIconState(
      GURL("https://example2.com/path/def_icon.png"));
  second_icon_state.bitmaps = {second_bitmap};
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate);

  // Second update - go back to the original.
  SetupBasicInstallablePageState();
  bool product_icon_fetched = false;
  web_contents_manager().GetOrCreateIconState(kDefaultIconUrl).on_icon_fetched =
      base::BindLambdaForTesting([&]() { product_icon_fetched = true; });
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);
  EXPECT_FALSE(product_icon_fetched);
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

  EXPECT_FALSE(base::PathExists(
      provider().icon_manager().GetAppPendingManifestIconDirForTesting(
          app_id)));
  EXPECT_FALSE(base::PathExists(
      provider().icon_manager().GetAppPendingTrustedIconDirForTesting(app_id)));
}

TEST_F(ManifestSilentUpdateCommandTest, UpdateWithIconWriteFailure) {
  // Have the silent update command triggered twice, where the second reverts
  // back to the original state, which should clear the pending update info.

  SetupBasicInstallablePageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  file_utils().SetRemainingDiskSpaceSize(0);

  blink::Manifest::ImageResource second_icon;
  second_icon.src = GURL("https://example2.com/path/def_icon.png");
  second_icon.sizes = {{96, 96}};
  second_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  GetPageManifest()->icons = {second_icon};
  SkBitmap second_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  auto& second_icon_state = web_contents_manager().GetOrCreateIconState(
      GURL("https://example2.com/path/def_icon.png"));
  second_icon_state.bitmaps = {second_bitmap};
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed);
  EXPECT_FALSE(AppHasPendingUpdateInfo(app_id));

  EXPECT_FALSE(base::PathExists(
      provider().icon_manager().GetAppPendingManifestIconDirForTesting(
          app_id)));
  EXPECT_FALSE(base::PathExists(
      provider().icon_manager().GetAppPendingTrustedIconDirForTesting(app_id)));
}

class ManifestSilentUpdateCommandExternalAppsTest
    : public ManifestSilentUpdateCommandTest,
      public testing::WithParamInterface<ExternalInstallSource> {
 public:
  webapps::AppId InstallExternallyManagedAppFromSource() {
    // This will always install external apps that open in a new browser tab.
    ExternalInstallOptions install_options(kAppUrl,
                                           /*user_display_mode=*/std::nullopt,
                                           GetParam());

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    provider().scheduler().InstallExternallyManagedApp(
        install_options,
        /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());
    const ExternallyManagedAppManager::InstallResult& result =
        future.Get<ExternallyManagedAppManager::InstallResult>();
    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_TRUE(result.app_id.has_value());
    return *result.app_id;
  }
};

TEST_P(ManifestSilentUpdateCommandExternalAppsTest, AppUpToDate) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kAppUpToDate,
                              /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest, StartUrlUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

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

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       AppNameChangedNoPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"New Name"));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       IconMoreThanTenPercentDiffChangedUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};

  // Set icon in content. Setting the icon color to YELLOW to trigger a more
  // than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));

  // Verify pending update icon bitmaps are not written to disk.
  EXPECT_FALSE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_FALSE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       IconLessThanTenPercentChangedDiffUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};

  SkBitmap changed_bitmap = gfx::test::CreateBitmap(96, SK_ColorCYAN);
  // For a 96x96 image, total pixels = 9216.
  // 10% of 9216 = 921.6 pixels.
  // We'll change a small area, for example, the first 9 rows, to a different
  // color. 9 rows * 96 columns = 864 pixels changed. This is < 10%.
  changed_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorRED);

  // Set icon in content.
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {changed_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  // Verify pending update icon bitmaps are not saved to disk.
  EXPECT_FALSE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_FALSE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       DoubleVisitsUpdateOnlyOnce) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon for the first visit.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  new_manifest->icons = {new_icon};

  // Set icon in content. Setting the icon color to YELLOW to trigger a more
  // than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  // Retrigger a manifest update for the same set of constraints, and verify
  // that an update does not happen.
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);
  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(
          base::Bucket(ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                       /*count=*/1),
          base::Bucket(ManifestSilentUpdateCheckResult::kAppUpToDate,
                       /*count=*/1)));
}

INSTANTIATE_TEST_SUITE_P(
    AllExternalInstallSources,
    ManifestSilentUpdateCommandExternalAppsTest,
    testing::Values(ExternalInstallSource::kInternalDefault,
                    ExternalInstallSource::kExternalDefault,
                    ExternalInstallSource::kExternalPolicy),
    [](const testing::TestParamInfo<ExternalInstallSource>& info) {
      switch (info.param) {
        case ExternalInstallSource::kInternalDefault:
          return "InternalDefault";
        case ExternalInstallSource::kExternalDefault:
          return "ExternalDefault";
        case ExternalInstallSource::kExternalPolicy:
          return "ExternalPolicy";
        default:
          NOTREACHED();
      }
    });

}  // namespace web_app
