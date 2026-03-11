// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/finalize_update_job.h"

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

class FinalizeUpdateJobWrapperCommand
    : public WebAppCommand<AppLock,
                           webapps::AppId,
                           webapps::InstallResultCode> {
 public:
  FinalizeUpdateJobWrapperCommand(
      WebAppProvider& provider,
      const WebAppInstallInfo& install_info,
      base::OnceCallback<void(webapps::AppId, webapps::InstallResultCode)>
          callback)
      : WebAppCommand<AppLock, webapps::AppId, webapps::InstallResultCode>(
            "FinalizeUpdateJobWrapperCommand",
            AppLockDescription(
                GenerateAppId(std::nullopt, install_info.start_url())),
            std::move(callback),
            std::make_tuple(webapps::AppId(),
                            webapps::InstallResultCode::
                                kCancelledOnWebAppProviderShuttingDown)),
        provider_(provider),
        install_info_(install_info.Clone()) {}

  void StartWithLock(std::unique_ptr<AppLock> lock) override {
    lock_ = std::move(lock);
    job_ = std::make_unique<FinalizeUpdateJob>(lock_.get(), lock_.get(),
                                               *provider_, install_info_);
    job_->Start(
        base::BindOnce(&FinalizeUpdateJobWrapperCommand::OnUpdateFinalized,
                       weak_factory_.GetWeakPtr()));
  }

  void OnUpdateFinalized(const webapps::AppId& app_id,
                         webapps::InstallResultCode code) {
    CompleteAndSelfDestruct(webapps::IsSuccess(code) ? CommandResult::kSuccess
                                                     : CommandResult::kFailure,
                            app_id, code);
  }

 private:
  raw_ref<WebAppProvider> provider_;
  WebAppInstallInfo install_info_;
  std::unique_ptr<AppLock> lock_;
  std::unique_ptr<FinalizeUpdateJob> job_;
  base::WeakPtrFactory<FinalizeUpdateJobWrapperCommand> weak_factory_{this};
};

}  // namespace

class TestInstallManagerObserver : public WebAppInstallManagerObserver {
 public:
  explicit TestInstallManagerObserver(WebAppInstallManager* install_manager) {
    install_manager_observation_.Observe(install_manager);
  }

  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override {
    web_app_manifest_updated_called_ = true;
  }

  bool web_app_manifest_updated_called() const {
    return web_app_manifest_updated_called_;
  }

 private:
  bool web_app_manifest_updated_called_ = false;
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};
};

class FinalizeUpdateJobTest : public WebAppTest {
 public:
  FinalizeUpdateJobTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
  }
  FinalizeUpdateJobTest(const FinalizeUpdateJobTest&) = delete;
  FinalizeUpdateJobTest& operator=(const FinalizeUpdateJobTest&) = delete;
  ~FinalizeUpdateJobTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    auto install_manager = std::make_unique<WebAppInstallManager>(profile());
    install_manager_observer_ =
        std::make_unique<TestInstallManagerObserver>(install_manager.get());
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
    install_manager_observer_.reset();
    WebAppTest::TearDown();
  }

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
  std::unique_ptr<TestInstallManagerObserver> install_manager_observer_;
  raw_ptr<MockWebAppCommandScheduler> mock_scheduler_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(FinalizeUpdateJobTest, OnWebAppManifestUpdatedTriggered) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";

  webapps::AppId app_id = test::InstallWebApp(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  base::test::TestFuture<webapps::AppId, webapps::InstallResultCode>
      update_future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<FinalizeUpdateJobWrapperCommand>(
          provider(), *info, update_future.GetCallback()));
  ASSERT_TRUE(update_future.Wait());
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());
}

TEST_F(FinalizeUpdateJobTest, ManifestUpdateOsIntegrationDefaultApps) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";

  webapps::AppId app_id = test::InstallWebAppWithoutOsIntegration(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id));

  base::test::TestFuture<webapps::AppId, webapps::InstallResultCode>
      update_future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<FinalizeUpdateJobWrapperCommand>(
          provider(), *info, update_future.GetCallback()));
  ASSERT_TRUE(update_future.Wait());
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());

  // Post manifest update, OS integration is not triggered for default apps.
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id));
}

TEST_F(FinalizeUpdateJobTest, InstallOsHooksDisabledForDefaultApps) {
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://foo.example"));
  info->title = u"Foo Title";

  webapps::AppId app_id = test::InstallWebAppWithoutOsIntegration(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);

  EXPECT_EQ(app_id, GenerateAppId(/*manifest_id_path=*/std::nullopt,
                                  info->start_url()));

  // Update the app, adding a file handler.
  std::vector<blink::mojom::ManifestFileHandlerPtr> file_handlers;
  AddFileHandler(&file_handlers);
  PopulateFileHandlerInfoFromManifest(file_handlers, info->start_url(),
                                      info.get());

  base::test::TestFuture<webapps::AppId, webapps::InstallResultCode>
      update_future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<FinalizeUpdateJobWrapperCommand>(
          provider(), *info, update_future.GetCallback()));
  auto [updated_app_id, code] = update_future.Take();
  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled, code);
}

TEST_F(FinalizeUpdateJobTest, MigrationSourceChangeSchedulesSync) {
  GURL start_url("https://foo.example");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Foo Title";
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetMigrationSourcesData(
          {webapps::ManifestId(GURL("https://migration.foo.example/"))});

  // 1. Install without migration sources.
  webapps::AppId app_id = test::InstallWebApp(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_fields=*/true,
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  // 2. Expect ScheduleResolveWebAppPendingMigrationInfo to be called.
  EXPECT_CALL(*mock_scheduler_, ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());

  // 3. Finalize update with migration sources.
  MigrationSource source(
      webapps::ManifestId(GURL("https://migration.foo.example/")),
      MigrationBehavior::kSuggest);
  info->migration_sources = {source};

  base::test::TestFuture<webapps::AppId, webapps::InstallResultCode>
      update_future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<FinalizeUpdateJobWrapperCommand>(
          provider(), *info, update_future.GetCallback()));
  ASSERT_TRUE(update_future.Wait());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            update_future.Get<webapps::InstallResultCode>());
}

}  // namespace web_app
