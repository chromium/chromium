// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/finalize_install_job.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/traits_bag.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/model/migration_source.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using testing::_;

struct FinalizeInstallResult {
  webapps::AppId installed_app_id;
  webapps::InstallResultCode code;
};

class MockWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  explicit MockWebAppCommandScheduler(Profile& profile)
      : WebAppCommandScheduler(profile) {}
  ~MockWebAppCommandScheduler() override = default;

  MOCK_METHOD(void,
              ScheduleResolveWebAppPendingMigrationInfo,
              (base::OnceClosure callback, const base::Location& location),
              (override));
};

class FinalizeInstallJobWrapperCommand
    : public WebAppCommand<AppLock,
                           webapps::AppId,
                           webapps::InstallResultCode> {
 public:
  FinalizeInstallJobWrapperCommand(
      Profile* profile,
      const WebAppInstallInfo& install_info,
      const FinalizeJobOptions& options,
      base::OnceCallback<void(webapps::AppId, webapps::InstallResultCode)>
          callback)
      : WebAppCommand<AppLock, webapps::AppId, webapps::InstallResultCode>(
            "FinalizeInstallJobWrapperCommand",
            AppLockDescription(
                GenerateAppId(std::nullopt, install_info.start_url())),
            std::move(callback),
            std::make_tuple(webapps::AppId(),
                            webapps::InstallResultCode::
                                kCancelledOnWebAppProviderShuttingDown)),
        profile_(profile),
        install_info_(install_info.Clone()),
        options_(options) {}

  void StartWithLock(std::unique_ptr<AppLock> lock) override {
    lock_ = std::move(lock);
    job_ = std::make_unique<FinalizeInstallJob>(
        *profile_, lock_.get(), lock_.get(), install_info_, options_);
    job_->Start(
        base::BindOnce(&FinalizeInstallJobWrapperCommand::OnInstallFinalized,
                       weak_factory_.GetWeakPtr()));
  }

  void OnInstallFinalized(const webapps::AppId& app_id,
                          webapps::InstallResultCode code) {
    CompleteAndSelfDestruct(webapps::IsSuccess(code) ? CommandResult::kSuccess
                                                     : CommandResult::kFailure,
                            app_id, code);
  }

 private:
  raw_ptr<Profile> profile_;
  WebAppInstallInfo install_info_;
  FinalizeJobOptions options_;
  std::unique_ptr<AppLock> lock_;
  std::unique_ptr<FinalizeInstallJob> job_;
  base::WeakPtrFactory<FinalizeInstallJobWrapperCommand> weak_factory_{this};
};

}  // namespace

class FinalizeInstallJobTest : public WebAppTest {
 public:
  FinalizeInstallJobTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
  }
  FinalizeInstallJobTest(const FinalizeInstallJobTest&) = delete;
  FinalizeInstallJobTest& operator=(const FinalizeInstallJobTest&) = delete;
  ~FinalizeInstallJobTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    auto install_manager = std::make_unique<WebAppInstallManager>(profile());
    provider->SetInstallManager(std::move(install_manager));
    provider->SetOriginAssociationManager(
        std::make_unique<FakeWebAppOriginAssociationManager>());

    auto mock_scheduler =
        std::make_unique<MockWebAppCommandScheduler>(*profile());
    mock_scheduler_ = mock_scheduler.get();
    // Expect (and flush at the end of this method) initially scheduled pending
    // migration sync command.
    EXPECT_CALL(*mock_scheduler_,
                ScheduleResolveWebAppPendingMigrationInfo(_, _))
        .WillOnce(base::test::RunOnceClosure<0>());
    provider->SetScheduler(std::move(mock_scheduler));

    test::AwaitStartWebAppProviderAndSubsystems(profile());

    provider->command_manager().AwaitAllCommandsCompleteForTesting();
    testing::Mock::VerifyAndClearExpectations(mock_scheduler_);
  }

  void TearDown() override {
    mock_scheduler_ = nullptr;
    WebAppTest::TearDown();
  }

  FinalizeInstallResult AwaitFinalizeInstall(
      const WebAppInstallInfo& info,
      const FinalizeJobOptions& options) {
    FinalizeInstallResult result{};
    base::test::TestFuture<webapps::AppId, webapps::InstallResultCode> future;
    provider().command_manager().ScheduleCommand(
        std::make_unique<FinalizeInstallJobWrapperCommand>(
            profile(), info, options, future.GetCallback()));
    result.code = future.Get<webapps::InstallResultCode>();
    result.installed_app_id = future.Get<webapps::AppId>();
    return result;
  }

  void AddFileHandler(
      std::vector<blink::mojom::ManifestFileHandlerPtr>* file_handlers) {
    auto file_handler = blink::mojom::ManifestFileHandler::New();
    file_handler->action = GURL("https://example.com/action");
    file_handler->name = u"Test handler";
    file_handler->accept[u"application/pdf"].emplace_back(u".pdf");
    file_handlers->push_back(std::move(file_handler));
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }
  WebAppInstallManager& install_manager() {
    return provider().install_manager();
  }

 protected:
  raw_ptr<MockWebAppCommandScheduler> mock_scheduler_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(FinalizeInstallJobTest, BasicInstallSucceeds) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));
}

TEST_F(FinalizeInstallJobTest, ConcurrentInstallSucceeds) {
  auto info1 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo1.example"));
  info1->title = u"Foo1 Title";

  auto info2 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo2.example"));
  info2->title = u"Foo2 Title";

  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  base::test::TestFuture<webapps::AppId, webapps::InstallResultCode> future1;
  base::test::TestFuture<webapps::AppId, webapps::InstallResultCode> future2;

  // Start install finalization for the 1st app.
  provider().command_manager().ScheduleCommand(
      std::make_unique<FinalizeInstallJobWrapperCommand>(
          profile(), *info1, options, future1.GetCallback()));

  // Start install finalization for the 2nd app.
  provider().command_manager().ScheduleCommand(
      std::make_unique<FinalizeInstallJobWrapperCommand>(
          profile(), *info2, options, future2.GetCallback()));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future1.Get<webapps::InstallResultCode>());
  EXPECT_EQ(
      future1.Get<webapps::AppId>(),
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info1->start_url()));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future2.Get<webapps::InstallResultCode>());
  EXPECT_EQ(
      future2.Get<webapps::AppId>(),
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info2->start_url()));
}

TEST_F(FinalizeInstallJobTest, InstallStoresLatestWebAppInstallSource) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
            *registrar().GetLatestAppInstallSource(result.installed_app_id));
}

TEST_F(FinalizeInstallJobTest, NonLocalThenLocalInstallSetsBothInstallTime) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.install_state = proto::SUGGESTED_FROM_ANOTHER_DEVICE;
  // OS Hooks must be disabled for non-locally installed app.
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(proto::SUGGESTED_FROM_ANOTHER_DEVICE,
              installed_app->install_state());
    EXPECT_TRUE(installed_app->first_install_time().is_null());
    EXPECT_TRUE(installed_app->latest_install_time().is_null());
  }

  options.install_state = proto::INSTALLED_WITH_OS_INTEGRATION;

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
              installed_app->install_state());
    EXPECT_FALSE(installed_app->first_install_time().is_null());
    EXPECT_FALSE(installed_app->latest_install_time().is_null());
    EXPECT_EQ(installed_app->first_install_time(),
              installed_app->latest_install_time());
  }
}

TEST_F(FinalizeInstallJobTest, LatestInstallTimeAlwaysUpdatedIfReinstalled) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_state = proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;

  base::Time old_first_install_time;
  base::Time old_latest_install_time;

  base::SimpleTestClock test_clock;
  provider().SetClockForTesting(&test_clock);
  auto toProtoResolutionTime = [](base::Time time) {
    return syncer::ProtoTimeToTime(syncer::TimeToProtoTime(time));
  };
  test_clock.SetNow(toProtoResolutionTime(base::Time::Now()));

  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(installed_app->install_state(),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    old_first_install_time = installed_app->first_install_time();
    old_latest_install_time = installed_app->latest_install_time();
    EXPECT_FALSE(old_first_install_time.is_null());
    EXPECT_FALSE(old_latest_install_time.is_null());
    EXPECT_EQ(old_first_install_time, old_latest_install_time);
    EXPECT_EQ(old_first_install_time, toProtoResolutionTime(test_clock.Now()));
  }
  test_clock.Advance(base::Hours(1));

  // Try reinstalling the same app again, the latest install time should be
  // updated but the first install time should still stay the same.
  {
    FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

    ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    const WebApp* installed_app =
        registrar().GetAppById(result.installed_app_id);

    EXPECT_EQ(installed_app->install_state(),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_FALSE(installed_app->first_install_time().is_null());
    EXPECT_FALSE(installed_app->latest_install_time().is_null());
    EXPECT_EQ(installed_app->first_install_time(), old_first_install_time);
    EXPECT_NE(installed_app->latest_install_time(), old_latest_install_time);
    EXPECT_EQ(installed_app->latest_install_time(),
              toProtoResolutionTime(test_clock.Now()));
  }
  // Reset the clock to the default clock, so raw_ptr issues don't happen.
  provider().SetClockForTesting(base::DefaultClock::GetInstance());
}

TEST_F(FinalizeInstallJobTest, InstallNoDesktopShortcut) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));
}

TEST_F(FinalizeInstallJobTest, InstallNoQuickLaunchBarShortcut) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_quick_launch_bar = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));
}

TEST_F(FinalizeInstallJobTest,
       InstallNoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));
}

TEST_F(FinalizeInstallJobTest, InstallNoCreateOsShorcuts) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));
}

TEST_F(FinalizeInstallJobTest, InstallOsHooksEnabledForUserInstalledApps) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  FinalizeJobOptions options(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));
}

TEST_F(FinalizeInstallJobTest, InstallUrlSetInWebAppDB) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";
  info->install_url = GURL("https://foo.example/installer");
  FinalizeJobOptions options(webapps::WebappInstallSource::EXTERNAL_POLICY);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));

  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  const WebApp::ExternalConfigMap& config_map =
      installed_app->management_to_external_config_map();
  auto it = config_map.find(WebAppManagement::kPolicy);
  EXPECT_NE(it, config_map.end());
  EXPECT_EQ(1u, it->second.install_urls.size());
  EXPECT_EQ(GURL("https://foo.example/installer"),
            *it->second.install_urls.begin());
}

TEST_F(FinalizeInstallJobTest, IsolationDataSetInWebAppDB) {
  IwaVersion version = *IwaVersion::Create("1.2.3");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      IwaOrigin(test::GetDefaultEcdsaP256WebBundleId()).origin().GetURL());
  info->title = u"Foo Title";
  info->set_isolated_web_app_version(version);

  const IsolatedWebAppStorageLocation location(
      IwaStorageUnownedBundle{base::FilePath(FILE_PATH_LITERAL("p"))});
  FinalizeJobOptions options(webapps::WebappInstallSource::EXTERNAL_POLICY);

  auto integrity_block_data =
      IsolatedWebAppIntegrityBlockData(test::CreateSignatures());
  options.iwa_options =
      FinalizeJobOptions::IwaOptions(location, integrity_block_data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(
      result.installed_app_id,
      GenerateAppId(/*manifest_id_path=*/std::nullopt, info->start_url()));

  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_THAT(
      installed_app,
      test::IwaIs(_, test::IsolationDataIs(location, version,
                                           /*controlled_frame_partitions=*/_,
                                           /*pending_update_info=*/std::nullopt,
                                           integrity_block_data)));
}

TEST_F(FinalizeInstallJobTest, PopUpContentSettingsGrantedForIwa) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle();
  app->TrustSigningKey();
  const web_app::IsolatedWebAppUrlInfo url_info =
      app->InstallChecked(profile());
  scoped_refptr<HostContentSettingsMap> host_content_settings_map(
      HostContentSettingsMapFactory::GetForProfile(profile()));

  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url_info.origin().GetURL(), url_info.origin().GetURL(),
                ContentSettingsType::POPUPS));
}

TEST_F(FinalizeInstallJobTest, ValidateOriginAssociationsApproved) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(start_url,
                                         /*has_origin_wildcard=*/true);
  CHECK(!scope_extension.origin.opaque());
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data = {
      {scope_extension, scope_extension}};
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(installed_app->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(ScopeExtensions({scope_extension}),
            installed_app->validated_scope_extensions());
}

TEST_F(FinalizeInstallJobTest, ValidateOriginAssociationsDenied) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(start_url,
                                         /*has_origin_wildcard=*/true);
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will not be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data;
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(installed_app->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(ScopeExtensions(), installed_app->validated_scope_extensions());
}

TEST_F(FinalizeInstallJobTest, ValidateMigrationSourcesApproved) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  MigrationSource source(
      webapps::ManifestId(GURL("https://migration.foo.example/")),
      MigrationBehavior::kSuggest);
  info->migration_sources = {source};

  // Set data such that migration source will be returned in validated data.
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetMigrationSourcesData(
          {webapps::ManifestId(GURL("https://migration.foo.example/"))});

  EXPECT_CALL(*mock_scheduler_, ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_THAT(
      installed_app->unvalidated_migration_sources(),
      testing::ElementsAre(testing::Property(
          &MigrationSource::manifest_id,
          webapps::ManifestId(GURL("https://migration.foo.example/")))));
  EXPECT_THAT(
      installed_app->validated_migration_sources(),
      testing::ElementsAre(testing::Property(
          &MigrationSource::manifest_id,
          webapps::ManifestId(GURL("https://migration.foo.example/")))));
}
TEST_F(FinalizeInstallJobTest,
       SuggestedFromMigrationSucceedsWithoutValidatedSource) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;

  MigrationSource source(
      webapps::ManifestId(GURL("https://migration.foo.example/")),
      MigrationBehavior::kSuggest);
  info->migration_sources = {source};

  // Set data such that migration source will NOT be returned in validated data.
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetMigrationSourcesData({});

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(proto::InstallState::SUGGESTED_FROM_MIGRATION,
            installed_app->install_state());
  EXPECT_THAT(
      installed_app->unvalidated_migration_sources(),
      testing::ElementsAre(testing::Property(
          &MigrationSource::manifest_id,
          webapps::ManifestId(GURL("https://migration.foo.example/")))));
  EXPECT_TRUE(installed_app->validated_migration_sources().empty());
}
TEST_F(FinalizeInstallJobTest,
       SuggestedFromMigrationFailsWithoutMigrationSources) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);
  options.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;

  info->migration_sources = {};

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kNoValidMigrationSource, result.code);
  EXPECT_FALSE(registrar().GetAppById(result.installed_app_id));
}

class FinalizeInstallJobTestQueriesAndFragments
    : public FinalizeInstallJobTest,
      public testing::WithParamInterface<std::tuple<std::string, std::string>> {
 public:
  FinalizeInstallJobTestQueriesAndFragments() = default;
  FinalizeInstallJobTestQueriesAndFragments(
      const FinalizeInstallJobTestQueriesAndFragments&) = delete;
  FinalizeInstallJobTestQueriesAndFragments& operator=(
      const FinalizeInstallJobTestQueriesAndFragments&) = delete;
  ~FinalizeInstallJobTestQueriesAndFragments() override = default;
};

TEST_P(FinalizeInstallJobTestQueriesAndFragments,
       ValidateOriginAssociationsDropQueriesAndFragments) {
  std::string start_url_str, expected_sanitized_start_url_str;
  std::tie(start_url_str, expected_sanitized_start_url_str) = GetParam();
  GURL start_url(start_url_str);
  GURL expected_sanitized_start_url(expected_sanitized_start_url_str);
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  FinalizeJobOptions options(webapps::WebappInstallSource::INTERNAL_DEFAULT);

  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(start_url,
                                         /*has_origin_wildcard=*/true);
  CHECK(!scope_extension.origin.opaque());
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data = {
      {scope_extension, scope_extension}};
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const WebApp* installed_app = registrar().GetAppById(result.installed_app_id);
  EXPECT_EQ(installed_app->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(ScopeExtensions({scope_extension}),
            installed_app->validated_scope_extensions());
  for (const auto& scope_ext_info :
       installed_app->validated_scope_extensions()) {
    ASSERT_EQ(expected_sanitized_start_url, scope_ext_info.scope);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FinalizeInstallJobTestQueriesAndFragments,
    testing::Values(
        std::tuple<std::string, std::string>("https://foo.example/path",
                                             "https://foo.example/path"),
        std::tuple<std::string, std::string>(
            "https://foo.example/search?q=querystring",
            "https://foo.example/search"),
        std::tuple<std::string, std::string>("https://foo.example/#hello",
                                             "https://foo.example"),
        std::tuple<std::string, std::string>(
            "https://foo.example/search?q=querystring#hello",
            "https://foo.example/search")));

}  // namespace web_app
