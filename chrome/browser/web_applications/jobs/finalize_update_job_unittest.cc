// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/finalize_update_job.h"

#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/model/migration_source.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
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
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
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

constexpr char kDefaultAppUrl[] = "https://foo.example";
constexpr char16_t kDefaultAppTitle[] = u"Foo Title";

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

  std::unique_ptr<WebAppInstallInfo> CreateAppInfo(std::string_view start_url,
                                                   std::u16string title) {
    auto info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(start_url));
    info->title = std::move(title);
    return info;
  }

  webapps::InstallResultCode RunFinalizeUpdateJob(
      const WebAppInstallInfo& info) {
    base::test::TestFuture<webapps::AppId, webapps::InstallResultCode> future;
    provider().command_manager().ScheduleCommand(
        std::make_unique<FinalizeUpdateJobWrapperCommand>(
            provider(), info, future.GetCallback()));

    return future.Get<webapps::InstallResultCode>();
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
  std::unique_ptr<TestInstallManagerObserver> install_manager_observer_;
  raw_ptr<MockWebAppCommandScheduler> mock_scheduler_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(FinalizeUpdateJobTest, OnWebAppManifestUpdatedTriggered) {
  auto info = CreateAppInfo(kDefaultAppUrl, kDefaultAppTitle);

  webapps::AppId app_id = test::InstallWebApp(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*info));
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());
}

class FinalizeUpdateJobTestIwa
    : public FinalizeUpdateJobTest,
      public testing::WithParamInterface<IsolatedWebAppStorageLocation> {
 protected:
  webapps::AppId InstallBaseIwa(const WebAppInstallInfo& info,
                                const IsolatedWebAppStorageLocation& location,
                                std::optional<IsolatedWebAppIntegrityBlockData>
                                    integrity_block_data = std::nullopt) {
    webapps::WebappInstallSource install_source =
        location.dev_mode() ? webapps::WebappInstallSource::IWA_DEV_UI
                            : webapps::WebappInstallSource::IWA_EXTERNAL_POLICY;

    FinalizeJobOptions options(install_source);
    options.iwa_options = FinalizeJobOptions::IwaOptions(
        location,
        integrity_block_data.value_or(
            IsolatedWebAppIntegrityBlockData(test::CreateSignatures())));

    base::test::TestFuture<webapps::AppId, webapps::InstallResultCode> future;
    FakeWebAppProvider::Get(profile())->install_finalizer().FinalizeInstall(
        info, options,
        base::BindOnce(
            [](base::OnceCallback<void(webapps::AppId,
                                       webapps::InstallResultCode)> callback,
               const webapps::AppId& app_id, webapps::InstallResultCode code) {
              std::move(callback).Run(app_id, code);
            },
            future.GetCallback()));

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
              future.Get<webapps::InstallResultCode>());
    return future.Get<webapps::AppId>();
  }

  void SetPendingUpdateState(const webapps::AppId& app_id,
                             const IsolatedWebAppStorageLocation& location,
                             const IwaVersion& update_version,
                             std::optional<IsolatedWebAppIntegrityBlockData>
                                 integrity_block = std::nullopt) {
    WebApp* app = FakeWebAppProvider::Get(profile())
                      ->GetRegistrarMutable()
                      .GetAppByIdMutable(app_id);
    ASSERT_TRUE(app);
    ASSERT_TRUE(app->isolation_data().has_value());
    app->SetIsolationData(
        IsolationData::Builder(*app->isolation_data())
            .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
                location, update_version, integrity_block))
            .Build());
  }
};

TEST_P(FinalizeUpdateJobTestIwa, IwaUpdateManifestUrlIgnoredInDevMode) {
  const IsolatedWebAppStorageLocation& location = GetParam();
  const GURL update_manifest_url =
      GURL("https://example.com/update_manifest.json");
  const GURL start_url =
      IwaOrigin(test::GetDefaultEcdsaP256WebBundleId()).origin().GetURL();
  const IwaVersion installed_version = *IwaVersion::Create("1.0.0");
  const IwaVersion update_version = *IwaVersion::Create("2.0.0");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = kDefaultAppTitle;
  info->set_isolated_web_app_version(installed_version);
  info->iwa_update_manifest_url = update_manifest_url;

  webapps::AppId app_id = InstallBaseIwa(*info, location);
  auto integrity_block_data =
      IsolatedWebAppIntegrityBlockData(test::CreateSignatures());
  SetPendingUpdateState(app_id, location, update_version, integrity_block_data);

  auto update_info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  update_info->title = u"Foo Title Update";
  update_info->set_isolated_web_app_version(update_version);
  update_info->iwa_update_manifest_url = update_manifest_url;

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*update_info));

  const WebApp* updated_app = registrar().GetAppById(app_id);
  EXPECT_EQ(updated_app->isolation_data()->version(), update_version);

  std::optional<GURL> expected_url =
      location.dev_mode() ? std::nullopt
                          : std::optional<GURL>(update_manifest_url);
  EXPECT_EQ(updated_app->isolation_data()->update_manifest_url(), expected_url);
  EXPECT_FALSE(
      updated_app->isolation_data()->pending_update_info().has_value());
  EXPECT_EQ(updated_app->isolation_data()->integrity_block_data(),
            integrity_block_data);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FinalizeUpdateJobTestIwa,
    testing::Values(IsolatedWebAppStorageLocation(IwaStorageOwnedBundle{
                        "dir", /*dev_mode=*/false}),
                    IsolatedWebAppStorageLocation(IwaStorageOwnedBundle{
                        "dir", /*dev_mode=*/true}),
                    IsolatedWebAppStorageLocation(IwaStorageUnownedBundle{
                        base::FilePath(FILE_PATH_LITERAL("p"))}),
                    IsolatedWebAppStorageLocation(IwaStorageProxy{
                        url::Origin::Create(GURL("http://localhost:1234"))})));

TEST_F(FinalizeUpdateJobTest, ManifestUpdateOsIntegrationDefaultApps) {
  auto info = CreateAppInfo(kDefaultAppUrl, kDefaultAppTitle);

  webapps::AppId app_id = test::InstallWebAppWithoutOsIntegration(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*info));
  EXPECT_TRUE(install_manager_observer_->web_app_manifest_updated_called());

  // Post manifest update, OS integration is not triggered for default apps.
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id));
}

TEST_F(FinalizeUpdateJobTest, InstallOsHooksDisabledForDefaultApps) {
  auto info = CreateAppInfo(kDefaultAppUrl, kDefaultAppTitle);

  webapps::AppId app_id = test::InstallWebAppWithoutOsIntegration(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);

  EXPECT_EQ(app_id, GenerateAppId(/*manifest_id_path=*/std::nullopt,
                                  info->start_url()));

  // Update the app, adding a file handler.
  std::vector<blink::mojom::ManifestFileHandlerPtr> file_handlers;
  AddFileHandler(&file_handlers);
  PopulateFileHandlerInfoFromManifest(file_handlers, info->start_url(),
                                      info.get());

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*info));
}

TEST_F(FinalizeUpdateJobTest, MigrationSourceChangeSchedulesSync) {
  auto info = CreateAppInfo(kDefaultAppUrl, kDefaultAppTitle);
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetMigrationSourcesData(
          {webapps::ManifestId(GURL("https://migration.foo.example/"))});

  // 1. Install without migration sources.
  webapps::AppId app_id = test::InstallWebApp(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  // 2. Expect ScheduleResolveWebAppPendingMigrationInfo to be called.
  EXPECT_CALL(*mock_scheduler_, ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());

  // 3. Finalize update with migration sources.
  MigrationSource source(
      webapps::ManifestId(GURL("https://migration.foo.example/")),
      MigrationBehavior::kSuggest);
  info->migration_sources = {source};

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*info));
}

TEST_F(FinalizeUpdateJobTest, ValidateShortcutsSanitizedOutsideScope) {
  auto info = CreateAppInfo(kDefaultAppUrl, kDefaultAppTitle);

  webapps::AppId app_id = test::InstallWebApp(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  WebAppShortcutsMenuItemInfo valid_shortcut;
  valid_shortcut.name = u"Valid";
  valid_shortcut.url = GURL("https://foo.example/shortcut");

  WebAppShortcutsMenuItemInfo invalid_shortcut;
  invalid_shortcut.name = u"Invalid";
  invalid_shortcut.url = GURL("https://bar.example/shortcut");

  info->shortcuts_menu_item_infos = {valid_shortcut, invalid_shortcut};

  // Provide dummy icon bitmaps for both shortcuts.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  IconBitmaps valid_bitmaps;
  valid_bitmaps.any[16] = bitmap;
  IconBitmaps invalid_bitmaps;
  invalid_bitmaps.any[16] = bitmap;
  info->shortcuts_menu_icon_bitmaps = {valid_bitmaps, invalid_bitmaps};

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*info));

  const WebApp* updated_app = registrar().GetAppById(app_id);
  ASSERT_EQ(1u, updated_app->shortcuts_menu_item_infos().size());
  EXPECT_EQ(u"Valid", updated_app->shortcuts_menu_item_infos()[0].name);
}

TEST_F(FinalizeUpdateJobTest, ValidateShortcutsKeptInExtendedScope) {
  auto info = CreateAppInfo(kDefaultAppUrl, kDefaultAppTitle);

  webapps::AppId app_id = test::InstallWebApp(
      profile(), std::make_unique<WebAppInstallInfo>(info->Clone()),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_POLICY);

  WebAppShortcutsMenuItemInfo extended_scope_shortcut;
  extended_scope_shortcut.name = u"Extended Scope";
  extended_scope_shortcut.url = GURL("https://bar.example/shortcut");

  info->shortcuts_menu_item_infos = {extended_scope_shortcut};
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  IconBitmaps extended_bitmaps;
  extended_bitmaps.any[16] = bitmap;
  info->shortcuts_menu_icon_bitmaps = {extended_bitmaps};

  // Add bar.example as a validated scope extension.
  auto scope_extension =
      ScopeExtensionInfo::CreateForScope(GURL("https://bar.example/"),
                                         /*has_origin_wildcard=*/true);
  info->scope_extensions = {scope_extension};

  // Set data such that scope_extension will be returned in validated data.
  std::map<ScopeExtensionInfo, ScopeExtensionInfo> data = {
      {scope_extension, scope_extension}};
  static_cast<FakeWebAppOriginAssociationManager&>(
      provider().origin_association_manager())
      .SetData(data);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            RunFinalizeUpdateJob(*info));

  const WebApp* updated_app = registrar().GetAppById(app_id);
  ASSERT_EQ(1u, updated_app->shortcuts_menu_item_infos().size());
  EXPECT_EQ(u"Extended Scope",
            updated_app->shortcuts_menu_item_infos()[0].name);
}

}  // namespace web_app
