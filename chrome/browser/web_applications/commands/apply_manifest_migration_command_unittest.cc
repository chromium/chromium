// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_manifest_migration_command.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scheduler/apply_manifest_migration_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using base::BucketsAre;

// Unit tests verifying successful app migrations.
class ApplyManifestMigrationCommandTest : public WebAppTest {
 public:
  // Options used to install the web app so that it can be set up for
  // various migration use-cases. These vary as per the test case, and
  // is hence constructed as a separate struct.
  struct InstallOptionsForMigration {
    proto::InstallState install_state =
        proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
    bool set_valid_migration_source = true;
    webapps::WebappInstallSource install_source =
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
  };

  ApplyManifestMigrationCommandTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
  }
  ~ApplyManifestMigrationCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->UseRealOsIntegrationManager();
    auto origin_association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    association_manager_ = origin_association_manager.get();
    provider->SetOriginAssociationManager(
        std::move(origin_association_manager));
    provider->StartWithSubsystems();
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider);
  }

  void TearDown() override {
    // Make the raw_ptr `null` to prevent it from dangling when the provider
    // shuts down as part of WebAppTest::TearDown().
    association_manager_ = nullptr;
    WebAppTest::TearDown();
  }

 protected:
  ApplyManifestMigrationResult RunMigrationAndGetResult(
      const webapps::AppId& from_app_id,
      const webapps::AppId& to_app_id,
      const MigrationBehavior migration_behavior =
          MigrationBehavior::kSuggest) {
    base::test::TestFuture<ApplyManifestMigrationResult> result_future;
    fake_provider().scheduler().ApplyManifestMigration(
        from_app_id, to_app_id, migration_behavior, /*keep_alive=*/nullptr,
        /*profile_keep_alive=*/nullptr, result_future.GetCallback());
    if (!result_future.Wait()) {
      // This avoids a crash if there is a timeout.
      return ApplyManifestMigrationResult::kSystemShutdown;
    }
    return result_future.Get();
  }

  SkBitmap CreateSolidColorIcon(int size, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size, size);
    bitmap.eraseColor(color);
    return bitmap;
  }

  // TODO(crbug.com/465762477): Create a struct of parameters to create the app
  // instead of passing them into the function one by one.
  webapps::AppId InstallAppWithInstallState(
      const GURL app_url,
      std::u16string name,
      std::map<SquareSizePx, SkBitmap> icon_map,
      InstallOptionsForMigration install_options) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    info->title = name;
    info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    info->icon_bitmaps.any = std::move(icon_map);
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;

    web_app::WebAppInstallParams params;
    params.install_state = install_options.install_state;
    bool do_os_integration = install_options.install_state ==
                             proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
    params.add_to_applications_menu = do_os_integration;
    params.add_to_desktop = do_os_integration;
    params.add_to_quick_launch_bar = do_os_integration;

    if (install_options.set_valid_migration_source) {
      web_app::proto::WebAppMigrationSource source;
      source.set_manifest_id("https://app.source.com/");
      source.set_behavior(
          proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST);
      info->migration_sources.push_back(std::move(source));
    }

    fake_provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        install_options.install_source, result.GetCallback(), params);
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    const webapps::AppId app_id = result.Get<webapps::AppId>();
    return app_id;
  }

  std::vector<base::Bucket> GetApplyMigrationHistograms() {
    return histogram_tester_.GetAllSamples("WebApp.Migration.ApplyResult");
  }

  bool IsMigratedAppSetForSync(const webapps::ManifestId& source_manifest_id,
                               const webapps::AppId& migrated_app_id) {
    const WebApp* migrated_app =
        fake_provider().registrar_unsafe().GetAppById(migrated_app_id);
    return migrated_app->IsSynced() &&
           migrated_app->sync_proto().has_migrated_from_manifest_id() &&
           migrated_app->sync_proto().migrated_from_manifest_id() ==
               source_manifest_id;
  }

  bool IsOsIntegrationSupported() {
#if BUILDFLAG(IS_CHROMEOS)
    return false;
#else
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  SkColor GetShortcutColor(const webapps::AppId& app_id,
                           const std::string& app_name) {
    if (!IsOsIntegrationSupported()) {
      return SK_ColorTRANSPARENT;
    }

#if BUILDFLAG(IS_WIN)
    std::optional<SkColor> desktop_color =
        fake_os_integration().GetShortcutIconTopLeftColor(
            profile(), fake_os_integration().desktop(), app_id, app_name);
    std::optional<SkColor> application_menu_icon_color =
        fake_os_integration().GetShortcutIconTopLeftColor(
            profile(), fake_os_integration().application_menu(), app_id,
            app_name);
    EXPECT_EQ(desktop_color.value(), application_menu_icon_color.value());
    return desktop_color.value();
#elif BUILDFLAG(IS_MAC)
    std::optional<SkColor> icon_color =
        fake_os_integration().GetShortcutIconTopLeftColor(
            profile(), fake_os_integration().chrome_apps_folder(), app_id,
            app_name);
    EXPECT_TRUE(icon_color.has_value());
    return icon_color.value();
#elif BUILDFLAG(IS_LINUX)
    std::optional<SkColor> icon_color =
        fake_os_integration().GetShortcutIconTopLeftColor(
            profile(), fake_os_integration().desktop(), app_id, app_name,
            kLauncherIconSize);
    EXPECT_TRUE(icon_color.has_value());
    return icon_color.value();
#else
    NOTREACHED() << "Shortcuts not supported for other OS";
#endif
  }

  FakeWebAppOriginAssociationManager& origin_association_manager() {
    return *association_manager_;
  }

 private:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<FakeWebAppOriginAssociationManager> association_manager_ = nullptr;
};

TEST_F(ApplyManifestMigrationCommandTest,
       SuccessDestinationAppAlreadyInstalled) {
  base::HistogramTester histogram_tester;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);
  const webapps::ManifestId& source_manifest_id =
      fake_provider().registrar_unsafe().GetAppManifestId(source_app_id);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app also with OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);

  // Set data such that migration source will be returned in validated data for
  // installing the destination app.
  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), install_options);

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  ASSERT_TRUE(destination_state.has_value());
  EXPECT_TRUE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  base::test::TestFuture<const webapps::AppId&, const webapps::AppId&> future;
  WebAppInstallManagerObserverAdapter observer(
      &fake_provider().install_manager());
  observer.SetWebAppMigratedDelegate(future.GetRepeatingCallback());

  // Trigger the command, and verify a successful migration.
  // Note: The FakeWebAppUiManager has launches fail for unit tests, the launch
  // is tested in the browser test.
  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult(source_app_id, destination_app_id);
  ASSERT_EQ(ApplyManifestMigrationResult::
                kAppMigrationAppliedSuccessfullyLaunchFailed,
            result);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<0>(), source_app_id);
  EXPECT_EQ(future.Get<1>(), destination_app_id);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(ApplyManifestMigrationResult::
                                  kAppMigrationAppliedSuccessfullyLaunchFailed,
                              1)));

  // Source app is not in the registrar, and has no OS integration left over.
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Destination app is in the registrar with no changes.
  ASSERT_TRUE(IsMigratedAppSetForSync(source_manifest_id, destination_app_id));
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      destination_app_id,
      WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }
}

TEST_F(ApplyManifestMigrationCommandTest,
       SuccessForcedMigrationFullyInstalled) {
  base::HistogramTester histogram_tester;
  const SkColor source_color = SK_ColorGREEN;
  const SkColor dest_color = SK_ColorRED;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, source_color);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);
  const webapps::ManifestId& source_manifest_id =
      fake_provider().registrar_unsafe().GetAppManifestId(source_app_id);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app as if it was suggested for migration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, dest_color);

  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), install_options);

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(destination_state.has_value());
  EXPECT_TRUE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  // Trigger the command, and verify a successful migration.
  // Note: The FakeWebAppUiManager has launches fail for unit tests, the launch
  // is tested in the browser test.
  ApplyManifestMigrationResult result = RunMigrationAndGetResult(
      source_app_id, destination_app_id, MigrationBehavior::kForce);
  ASSERT_EQ(ApplyManifestMigrationResult::
                kAppMigrationAppliedSuccessfullyLaunchFailed,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(ApplyManifestMigrationResult::
                                  kAppMigrationAppliedSuccessfullyLaunchFailed,
                              1)));

  // Source app is not in the registrar, and has no OS integration left over.
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Destination app is in the registrar with full OS integration, and matching
  // the name and icon of the source app.
  ASSERT_TRUE(IsMigratedAppSetForSync(source_manifest_id, destination_app_id));
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      destination_app_id,
      WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id, base::UTF16ToUTF8(source_app_name)));
    EXPECT_EQ(source_color,
              GetShortcutColor(destination_app_id,
                               base::UTF16ToUTF8(source_app_name)));
  }
}

TEST_F(ApplyManifestMigrationCommandTest,
       SuccessForcedMigrationNotFullyInstalled) {
  base::HistogramTester histogram_tester;
  const SkColor source_color = SK_ColorGREEN;
  const SkColor dest_color = SK_ColorRED;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, source_color);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);
  const webapps::ManifestId& source_manifest_id =
      fake_provider().registrar_unsafe().GetAppManifestId(source_app_id);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app as if it was suggested for migration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, dest_color);

  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  install_options.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), install_options);

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(destination_state.has_value());
  EXPECT_FALSE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  // Trigger the command, and verify a successful migration.
  // Note: The FakeWebAppUiManager has launches fail for unit tests, the launch
  // is tested in the browser test.
  ApplyManifestMigrationResult result = RunMigrationAndGetResult(
      source_app_id, destination_app_id, MigrationBehavior::kForce);
  ASSERT_EQ(ApplyManifestMigrationResult::
                kAppMigrationAppliedSuccessfullyLaunchFailed,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(ApplyManifestMigrationResult::
                                  kAppMigrationAppliedSuccessfullyLaunchFailed,
                              1)));

  // Source app is not in the registrar, and has no OS integration left over.
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Destination app is in the registrar with full OS integration, and matching
  // the name and icon of the source app.
  ASSERT_TRUE(IsMigratedAppSetForSync(source_manifest_id, destination_app_id));
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      destination_app_id,
      WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id, base::UTF16ToUTF8(source_app_name)));
    EXPECT_EQ(source_color,
              GetShortcutColor(destination_app_id,
                               base::UTF16ToUTF8(source_app_name)));
  }
}

TEST_F(ApplyManifestMigrationCommandTest, SuccessSuggestedForMigration) {
  base::HistogramTester histogram_tester;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);
  const webapps::ManifestId& source_manifest_id =
      fake_provider().registrar_unsafe().GetAppManifestId(source_app_id);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app as if it was suggested for migration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);

  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  install_options.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), install_options);

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(destination_state.has_value());
  EXPECT_FALSE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  // Trigger the command, and verify a successful migration.
  // Note: The FakeWebAppUiManager has launches fail for unit tests, the launch
  // is tested in the browser test.
  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult(source_app_id, destination_app_id);
  ASSERT_EQ(ApplyManifestMigrationResult::
                kAppMigrationAppliedSuccessfullyLaunchFailed,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(ApplyManifestMigrationResult::
                                  kAppMigrationAppliedSuccessfullyLaunchFailed,
                              1)));

  // Source app is not in the registrar, and has no OS integration left over.
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Destination app is in the registrar with full OS integration.
  ASSERT_TRUE(IsMigratedAppSetForSync(source_manifest_id, destination_app_id));
  EXPECT_TRUE(fake_provider().registrar_unsafe().AppMatches(
      destination_app_id,
      WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }
}

TEST_F(ApplyManifestMigrationCommandTest, RunOnOsLoginMigrated) {
  base::HistogramTester histogram_tester;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);
  const webapps::ManifestId& source_manifest_id =
      fake_provider().registrar_unsafe().GetAppManifestId(source_app_id);

  // Set up Run on OS login for the web app to be opened in a windowed mode.
  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      source_app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->has_run_on_os_login());
  EXPECT_EQ(proto::os_state::RunOnOsLogin::MODE_WINDOWED,
            state->run_on_os_login().run_on_os_login_mode());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsRunOnOsLoginEnabled(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app as if it was suggested for migration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);

  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  install_options.install_state =
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), install_options);

  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsRunOnOsLoginEnabled(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  // Trigger the command, and verify a successful migration.
  // Note: The FakeWebAppUiManager has launches fail for unit tests, the launch
  // is tested in the browser test.
  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult(source_app_id, destination_app_id);
  ASSERT_EQ(ApplyManifestMigrationResult::
                kAppMigrationAppliedSuccessfullyLaunchFailed,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(ApplyManifestMigrationResult::
                                  kAppMigrationAppliedSuccessfullyLaunchFailed,
                              1)));

  // Source app is not in the registrar, and has no OS integration for run on OS
  // login left over.
  EXPECT_FALSE(fake_provider().registrar_unsafe().AppMatches(
      source_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  if (IsOsIntegrationSupported()) {
    EXPECT_FALSE(fake_os_integration().IsRunOnOsLoginEnabled(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Destination app is in the registrar with full OS integration.
  ASSERT_TRUE(IsMigratedAppSetForSync(source_manifest_id, destination_app_id));
  auto dest_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(dest_state.has_value());
  EXPECT_TRUE(dest_state->has_run_on_os_login());
  EXPECT_EQ(proto::os_state::RunOnOsLogin::MODE_WINDOWED,
            state->run_on_os_login().run_on_os_login_mode());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsRunOnOsLoginEnabled(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }
}

TEST_F(ApplyManifestMigrationCommandTest, DoNotSetValidatedSources) {
  base::HistogramTester histogram_tester;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app also with OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);

  // Set data such that migration source will be returned in validated data for
  // installing the destination app.
  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), {.set_valid_migration_source = false});

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(destination_state.has_value());
  EXPECT_TRUE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult(source_app_id, destination_app_id);
  ASSERT_EQ(ApplyManifestMigrationResult::kDestinationAppDoesNotLinkToSourceApp,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(
          ApplyManifestMigrationResult::kDestinationAppDoesNotLinkToSourceApp,
          1)));
}

TEST_F(ApplyManifestMigrationCommandTest, SourceAppPolicyInstalled) {
  base::HistogramTester histogram_tester;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      {.set_valid_migration_source = false,
       .install_source = webapps::WebappInstallSource::EXTERNAL_POLICY});

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  // Install the destination app also with OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);

  // Set data such that migration source will be returned in validated data for
  // installing the destination app.
  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), {.set_valid_migration_source = false});

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(destination_state.has_value());
  EXPECT_TRUE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult(source_app_id, destination_app_id);
  ASSERT_EQ(ApplyManifestMigrationResult::kSourceAppInvalidForMigration,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(
          ApplyManifestMigrationResult::kSourceAppInvalidForMigration, 1)));
}

TEST_F(ApplyManifestMigrationCommandTest, NoSourceApp) {
  base::HistogramTester histogram_tester;
  // Install the destination app with OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map2;
  std::u16string destination_app_name = u"Destination app";
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorRED);

  // Set data such that migration source will be returned in validated data for
  // installing the destination app.
  origin_association_manager().SetMigrationSourcesData(
      {webapps::ManifestId(GURL("https://app.source.com/"))});

  const webapps::AppId& destination_app_id = InstallAppWithInstallState(
      GURL("https://app.destination.com/"), destination_app_name,
      std::move(icon_map2), {.set_valid_migration_source = false});

  auto destination_state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          destination_app_id);
  EXPECT_TRUE(destination_state.has_value());
  EXPECT_TRUE(destination_state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), destination_app_id,
        base::UTF16ToUTF8(destination_app_name)));
  }

  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult("random_app", destination_app_id);
  ASSERT_EQ(ApplyManifestMigrationResult::kSourceAppInvalidForMigration,
            result);

  EXPECT_THAT(
      GetApplyMigrationHistograms(),
      BucketsAre(base::Bucket(
          ApplyManifestMigrationResult::kSourceAppInvalidForMigration, 1)));
}

TEST_F(ApplyManifestMigrationCommandTest, NoDestinationApp) {
  base::HistogramTester histogram_tester;
  // Install the source app first with complete OS integration.
  std::map<SquareSizePx, SkBitmap> icon_map;
  std::u16string source_app_name = u"Source app";
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);

  InstallOptionsForMigration install_options;
  const webapps::AppId& source_app_id = InstallAppWithInstallState(
      GURL("https://app.source.com/"), source_app_name, std::move(icon_map),
      install_options);

  auto state =
      fake_provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          source_app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state.value().has_shortcut());
  if (IsOsIntegrationSupported()) {
    EXPECT_TRUE(fake_os_integration().IsShortcutCreated(
        profile(), source_app_id, base::UTF16ToUTF8(source_app_name)));
  }

  ApplyManifestMigrationResult result =
      RunMigrationAndGetResult(source_app_id, "random_destination");
  ASSERT_EQ(ApplyManifestMigrationResult::kDestinationAppInvalid, result);

  EXPECT_THAT(GetApplyMigrationHistograms(),
              BucketsAre(base::Bucket(
                  ApplyManifestMigrationResult::kDestinationAppInvalid, 1)));
}

}  // namespace
}  // namespace web_app
