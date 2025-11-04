// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_names.h"
#endif

namespace web_app {

namespace {

Registry CreateRegistryForTesting(const std::string& base_url, int num_apps) {
  Registry registry;

  for (int i = 0; i < num_apps; ++i) {
    const auto url = base_url + base::NumberToString(i);
    const webapps::AppId app_id =
        GenerateAppId(/*manifest_id=*/std::nullopt, GURL(url));

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(WebAppManagement::kSync);
    web_app->SetStartUrl(GURL(url));
    web_app->SetName("Name" + base::NumberToString(i));
    web_app->SetDisplayMode(DisplayMode::kBrowser);
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    web_app->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
    // Set an OS integration state (with shortcuts) to prevent migration to a
    // partially installed status.
    proto::os_state::WebAppOsIntegration os_state;
    os_state.mutable_shortcut();
    web_app->SetCurrentOsIntegrationStates(os_state);

    registry.emplace(app_id, std::move(web_app));
  }

  return registry;
}

int CountApps(const WebAppRegistrar::AppSet& app_set) {
  int count = 0;
  for (const auto& web_app : app_set) {
    EXPECT_FALSE(web_app.is_uninstalling());
    ++count;
  }
  return count;
}

}  // namespace

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

// TODO(dmurph): Make this test run from the default FakeWebAppProvider like all
// other unittests.
class WebAppRegistrarTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    web_app::FakeWebAppProvider::Get(profile())->SetDatabaseFactory(
        std::make_unique<FakeWebAppDatabaseFactory>());
  }

  void TearDown() override {
    WebAppTest::TearDown();
  }

 protected:
  void StartWebAppProvider() {
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  base::flat_set<webapps::AppId> PopulateRegistry(const Registry& registry) {
    base::flat_set<webapps::AppId> app_ids;
    for (auto& kv : registry) {
      app_ids.insert(kv.second->app_id());
    }

    database_factory().WriteRegistry(registry);

    return app_ids;
  }

  void PopulateRegistryWithApp(std::unique_ptr<WebApp> app) {
    Registry registry;
    webapps::AppId app_id = app->app_id();
    registry[app_id] = std::move(app);
    PopulateRegistry(std::move(registry));
  }

  base::flat_set<webapps::AppId> PopulateRegistryWithApps(
      const std::string& base_url,
      int num_apps) {
    return PopulateRegistry(CreateRegistryForTesting(base_url, num_apps));
  }

  FakeWebAppDatabaseFactory& database_factory() const {
    return static_cast<FakeWebAppDatabaseFactory&>(
        fake_provider().GetDatabaseFactory());
  }

  WebAppRegistrar& registrar() const {
    return fake_provider().registrar_unsafe();
  }

  WebAppSyncBridge& sync_bridge() const {
    return fake_provider().sync_bridge_unsafe();
  }

  // Do not copy/paste this, and instead use normal installation commands to
  // install an app.
  void RegisterAppUnsafe(std::unique_ptr<WebApp> app) {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(app));
  }

  void Uninstall(const webapps::AppId& app_id) {
    // There is no longer a universal uninstall, so just remove each management.
    WebAppManagementTypes managements =
        registrar().GetAppById(app_id)->GetSources();
    for (WebAppManagement::Type type : managements) {
      base::test::TestFuture<webapps::UninstallResultCode> future;
      fake_provider().scheduler().RemoveInstallManagementMaybeUninstall(
          app_id, type, webapps::WebappUninstallSource::kTestCleanup,
          future.GetCallback());
      EXPECT_TRUE(future.Wait());
      EXPECT_EQ(future.Get<webapps::UninstallResultCode>(),
                webapps::UninstallResultCode::kAppRemoved);
    }
  }

  void UninstallViaRemoveSource(
      const webapps::AppId& app_id,
      web_app::WebAppManagement::Type management_type) {
    base::test::TestFuture<webapps::UninstallResultCode> future;
    fake_provider().scheduler().RemoveInstallManagementMaybeUninstall(
        app_id, management_type, webapps::WebappUninstallSource::kTestCleanup,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(future.Get<webapps::UninstallResultCode>(),
              webapps::UninstallResultCode::kAppRemoved);
  }

 private:
  std::unique_ptr<FakeWebAppDatabaseFactory> database_factory_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
};

class WebAppRegistrarTest_TabStrip : public WebAppRegistrarTest {
 public:
  WebAppRegistrarTest_TabStrip() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kDesktopPWAsTabStrip};
};

TEST_F(WebAppRegistrarTest, EmptyRegistrar) {
  StartWebAppProvider();
  EXPECT_TRUE(registrar().is_empty());
  EXPECT_FALSE(registrar().IsInRegistrar(webapps::AppId()));
  EXPECT_EQ(std::nullopt, registrar().GetInstallState(webapps::AppId()));
  EXPECT_FALSE(registrar().IsInRegistrar(webapps::AppId()));
  EXPECT_EQ(nullptr, registrar().GetAppById(webapps::AppId()));
  EXPECT_EQ(std::string(), registrar().GetAppShortName(webapps::AppId()));
  EXPECT_EQ(GURL(), registrar().GetAppStartUrl(webapps::AppId()));
}

TEST_F(WebAppRegistrarTest, InitWithApps) {
  const GURL start_url = GURL("https://example.com/path");
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const std::optional<SkColor> theme_color = 0xAABBCCDD;

  const GURL start_url2 = GURL("https://example.com/path2");
  const webapps::AppId app_id2 =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url2);

  auto web_app = std::make_unique<WebApp>(app_id);
  auto web_app2 = std::make_unique<WebApp>(app_id2);

  web_app->AddSource(WebAppManagement::kUserInstalled);
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetStartUrl(start_url);
  web_app->SetScope(scope);
  web_app->SetThemeColor(theme_color);

  web_app2->AddSource(WebAppManagement::kDefault);
  web_app2->SetDisplayMode(DisplayMode::kBrowser);
  web_app2->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  web_app2->SetStartUrl(start_url2);
  web_app2->SetName(name);

  Registry registry;
  registry[app_id] = std::move(web_app);
  registry[app_id2] = std::move(web_app2);
  PopulateRegistry(std::move(registry));

  StartWebAppProvider();

  EXPECT_TRUE(registrar().IsInRegistrar(app_id));
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id));

  const WebApp* app = registrar().GetAppById(app_id);

  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(name, app->untranslated_name());
  EXPECT_EQ(description, app->untranslated_description());
  EXPECT_EQ(start_url, app->start_url());
  EXPECT_EQ(scope, app->scope());
  EXPECT_EQ(theme_color, app->theme_color());

  EXPECT_FALSE(registrar().is_empty());

  EXPECT_TRUE(registrar().IsInRegistrar(app_id));
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id2));
  const WebApp* app2 = registrar().GetAppById(app_id2);
  EXPECT_EQ(app_id2, app2->app_id());
  EXPECT_FALSE(registrar().is_empty());
  EXPECT_EQ(CountApps(registrar().GetApps()), 2);

  Uninstall(app_id);
  EXPECT_FALSE(registrar().IsInRegistrar(app_id));
  EXPECT_EQ(std::nullopt, registrar().GetInstallState(app_id));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id));
  EXPECT_FALSE(registrar().is_empty());

  // Check that app2 is still registered.
  app2 = registrar().GetAppById(app_id2);
  EXPECT_TRUE(registrar().IsInRegistrar(app_id2));
  EXPECT_EQ(proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
            registrar().GetInstallState(app_id2));
  EXPECT_EQ(app_id2, app2->app_id());

  Uninstall(app_id2);
  EXPECT_FALSE(registrar().IsInRegistrar(app_id2));
  EXPECT_EQ(std::nullopt, registrar().GetInstallState(app_id2));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_TRUE(registrar().is_empty());
  EXPECT_EQ(CountApps(registrar().GetApps()), 0);
}

TEST_F(WebAppRegistrarTest, InitRegistrarAndDoForEachApp) {
  base::flat_set<webapps::AppId> ids = PopulateRegistry(
      CreateRegistryForTesting("https://example.com/path", 20));
  StartWebAppProvider();

  for (const WebApp& web_app : registrar().GetAppsIncludingStubs()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, DoForEachAndUnregisterAllApps) {
  Registry registry = CreateRegistryForTesting("https://example.com/path", 20);
  auto ids = PopulateRegistry(std::move(registry));
  EXPECT_EQ(20UL, ids.size());

  StartWebAppProvider();

  for (const WebApp& web_app : registrar().GetAppsIncludingStubs()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  EXPECT_FALSE(registrar().is_empty());
  test::UninstallAllWebApps(profile());
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, AppsInstalledByUserMetric) {
  base::HistogramTester histogram_tester;

  // All of these apps are marked as 'not locally installed'.
  PopulateRegistry(CreateRegistryForTesting("https://example.com/path", 10));
  StartWebAppProvider();

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.InstalledCount.ByUser"),
              base::BucketsAre(base::Bucket(/*min=*/10,
                                            /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.InstalledCount.ByUserNotLocallyInstalled"),
              base::BucketsAre(base::Bucket(/*min=*/0,
                                            /*count=*/1)));
}

TEST_F(WebAppRegistrarTest, AppsNonUserInstalledMetric) {
  base::HistogramTester histogram_tester;

  auto web_app = std::make_unique<WebApp>("app_id");
  web_app->AddSource(WebAppManagement::kPolicy);
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetName("name");
  web_app->SetStartUrl(GURL("https://example.com/path"));
  PopulateRegistryWithApp(std::move(web_app));
  StartWebAppProvider();

  histogram_tester.ExpectUniqueSample("WebApp.InstalledCount.ByUser",
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstalledCount.ByUserNotLocallyInstalled", /*sample=*/0,
      /*expected_bucket_count=*/1);
}

TEST_F(WebAppRegistrarTest, AppsNotLocallyInstalledMetric) {
  base::HistogramTester histogram_tester;

  auto web_app = std::make_unique<WebApp>("app_id");
  web_app->AddSource(WebAppManagement::kSync);
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetName("name");
  web_app->SetStartUrl(GURL("https://example.com/path"));
  web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
  PopulateRegistryWithApp(std::move(web_app));
  StartWebAppProvider();

  histogram_tester.ExpectUniqueSample("WebApp.InstalledCount.ByUser",
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstalledCount.ByUserNotLocallyInstalled", /*sample=*/1,
      /*expected_bucket_count=*/1);
}

TEST_F(WebAppRegistrarTest, GetApps) {
  base::flat_set<webapps::AppId> ids =
      PopulateRegistryWithApps("https://example.com/path", 10);
  StartWebAppProvider();

  int not_in_sync_install_count = 0;
  for (const WebApp& web_app : registrar().GetApps()) {
    ++not_in_sync_install_count;
    EXPECT_TRUE(base::Contains(ids, web_app.app_id()));
  }
  EXPECT_EQ(10, not_in_sync_install_count);

  auto web_app_in_sync1 = test::CreateWebApp(GURL("https://example.org/sync1"));
  web_app_in_sync1->SetIsFromSyncAndPendingInstallation(true);
  const webapps::AppId web_app_id_in_sync1 = web_app_in_sync1->app_id();
  RegisterAppUnsafe(std::move(web_app_in_sync1));

  auto web_app_in_sync2 = test::CreateWebApp(GURL("https://example.org/sync2"));
  web_app_in_sync2->SetIsFromSyncAndPendingInstallation(true);
  const webapps::AppId web_app_id_in_sync2 = web_app_in_sync2->app_id();
  RegisterAppUnsafe(std::move(web_app_in_sync2));

  int all_apps_count = 0;
  for ([[maybe_unused]] const WebApp& web_app :
       registrar().GetAppsIncludingStubs()) {
    ++all_apps_count;
  }
  EXPECT_EQ(12, all_apps_count);

  for (const WebApp& web_app : registrar().GetApps()) {
    EXPECT_NE(web_app_id_in_sync1, web_app.app_id());
    EXPECT_NE(web_app_id_in_sync2, web_app.app_id());

    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  Uninstall(web_app_id_in_sync1);
  Uninstall(web_app_id_in_sync2);

  not_in_sync_install_count = 0;
  for ([[maybe_unused]] const WebApp& web_app : registrar().GetApps()) {
    ++not_in_sync_install_count;
  }
  EXPECT_EQ(10, not_in_sync_install_count);
}

TEST_F(WebAppRegistrarTest, GetAppDataFields) {

  const GURL start_url = GURL("https://example.com/path");
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const std::optional<SkColor> theme_color = 0xAABBCCDD;
  const auto display_mode = DisplayMode::kMinimalUi;
  const auto user_display_mode = mojom::UserDisplayMode::kStandalone;
  std::vector<DisplayMode> display_mode_override;

  auto web_app = std::make_unique<WebApp>(app_id);

  display_mode_override.push_back(DisplayMode::kMinimalUi);
  display_mode_override.push_back(DisplayMode::kStandalone);

  web_app->AddSource(WebAppManagement::kSync);
  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetThemeColor(theme_color);
  web_app->SetStartUrl(start_url);
  web_app->SetDisplayMode(display_mode);
  web_app->SetUserDisplayMode(user_display_mode);
  web_app->SetDisplayModeOverride(display_mode_override);
  web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);

  PopulateRegistryWithApp(std::move(web_app));
  StartWebAppProvider();

  EXPECT_EQ(name, registrar().GetAppShortName(app_id));
  EXPECT_EQ(description, registrar().GetAppDescription(app_id));
  EXPECT_EQ(theme_color, registrar().GetAppThemeColor(app_id));
  EXPECT_EQ(start_url, registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            registrar().GetAppUserDisplayMode(app_id));

  {
    std::vector<DisplayMode> app_display_mode_override =
        registrar().GetAppDisplayModeOverride(app_id);
    ASSERT_EQ(2u, app_display_mode_override.size());
    EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[0]);
    EXPECT_EQ(DisplayMode::kStandalone, app_display_mode_override[1]);
  }

  {
    EXPECT_TRUE(registrar().IsInRegistrar(app_id));
    EXPECT_EQ(proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
              registrar().GetInstallState(app_id));

    EXPECT_FALSE(registrar().IsInRegistrar("unknown"));
    EXPECT_EQ(std::nullopt, registrar().GetInstallState("unknown"));
    base::test::TestFuture<void> future;
    fake_provider().scheduler().InstallAppLocally(app_id, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
              registrar().GetInstallState(app_id));
    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              registrar().GetAppUserDisplayMode(app_id));
  }

  {
    EXPECT_FALSE(registrar().GetAppUserDisplayMode("unknown").has_value());
    {
      ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
      update->UpdateApp(app_id)->SetUserDisplayMode(
          mojom::UserDisplayMode::kBrowser);
    }
    EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
              registrar().GetAppUserDisplayMode(app_id));

    fake_provider().sync_bridge_unsafe().SetAppUserDisplayModeForTesting(
        app_id, mojom::UserDisplayMode::kStandalone);
    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              registrar().GetAppUserDisplayMode(app_id));
    EXPECT_EQ(DisplayMode::kMinimalUi, registrar().GetAppDisplayMode(app_id));

    ASSERT_EQ(2u, registrar().GetAppDisplayModeOverride(app_id).size());
    EXPECT_EQ(DisplayMode::kMinimalUi,
              registrar().GetAppDisplayModeOverride(app_id)[0]);
    EXPECT_EQ(DisplayMode::kStandalone,
              registrar().GetAppDisplayModeOverride(app_id)[1]);
  }
}

TEST_F(WebAppRegistrarTest, CanFindAppsInScope) {
  StartWebAppProvider();

  const GURL origin_scope("https://example.com/");

  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");

  const webapps::AppId app1_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app1_scope);
  const webapps::AppId app2_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app2_scope);
  const webapps::AppId app3_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app3_scope);

  std::vector<webapps::AppId> in_scope = registrar().FindAllAppsNestedInUrl(
      origin_scope,
      web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  EXPECT_EQ(0u, in_scope.size());
  // TODO(crbug.com/340952100): Evaluate call sites of DoesScopeContainAnyApp
  // for correctness (note: multiple instances within this function).
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(
      origin_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                     proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(
      app3_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  auto app1 = test::CreateWebApp(app1_scope);
  app1->SetScope(app1_scope);
  RegisterAppUnsafe(std::move(app1));

  in_scope = registrar().FindAllAppsNestedInUrl(
      origin_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      origin_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                     proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(
      app3_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  in_scope = registrar().FindAllAppsNestedInUrl(
      app1_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      app1_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  auto app2 = test::CreateWebApp(app2_scope);
  app2->SetScope(app2_scope);
  RegisterAppUnsafe(std::move(app2));

  in_scope = registrar().FindAllAppsNestedInUrl(
      origin_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id, app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      origin_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                     proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(
      app3_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  in_scope = registrar().FindAllAppsNestedInUrl(
      app1_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id, app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      app1_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  in_scope = registrar().FindAllAppsNestedInUrl(
      app2_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      app2_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  auto app3 = test::CreateWebApp(app3_scope);
  app3->SetScope(app3_scope);
  RegisterAppUnsafe(std::move(app3));

  in_scope = registrar().FindAllAppsNestedInUrl(
      origin_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id, app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      origin_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                     proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));

  in_scope = registrar().FindAllAppsNestedInUrl(
      app3_scope, web_app::WebAppFilter::InstalledInChrome());
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app3_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(
      app3_scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION}));
}

TEST_F(WebAppRegistrarTest, CanFindAppWithUrlInScope) {
  StartWebAppProvider();

  const GURL origin_scope("https://example.com/");

  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");
  const GURL app4_scope("https://app-four.com/");

  const webapps::AppId app1_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app1_scope);
  const webapps::AppId app2_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app2_scope);
  const webapps::AppId app3_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app3_scope);
  const webapps::AppId app4_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app3_scope);

  auto app1 = test::CreateWebApp(app1_scope);
  app1->SetScope(app1_scope);
  RegisterAppUnsafe(std::move(app1));

  std::optional<webapps::AppId> app2_match =
      registrar().FindBestAppWithUrlInScope(
          app2_scope, web_app::WebAppFilter::InstalledInChrome());
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, app1_id);

  std::optional<webapps::AppId> app3_match =
      registrar().FindBestAppWithUrlInScope(
          app3_scope,
          web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  EXPECT_FALSE(app3_match);

  std::optional<webapps::AppId> app4_match =
      registrar().FindBestAppWithUrlInScope(
          app4_scope,
          web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  EXPECT_FALSE(app4_match);

  auto app2 = test::CreateWebApp(app2_scope);
  app2->SetScope(app2_scope);
  RegisterAppUnsafe(std::move(app2));

  auto app3 = test::CreateWebApp(app3_scope);
  app3->SetScope(app3_scope);
  RegisterAppUnsafe(std::move(app3));

  auto app4 = test::CreateWebApp(app4_scope);
  app4->SetScope(app4_scope);
  app4->SetIsUninstalling(true);
  RegisterAppUnsafe(std::move(app4));

  std::optional<webapps::AppId> origin_match =
      registrar().FindBestAppWithUrlInScope(
          origin_scope,
          web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  EXPECT_FALSE(origin_match);

  std::optional<webapps::AppId> app1_match =
      registrar().FindBestAppWithUrlInScope(
          app1_scope, web_app::WebAppFilter::InstalledInChrome());
  DCHECK(app1_match);
  EXPECT_EQ(*app1_match, app1_id);

  app2_match = registrar().FindBestAppWithUrlInScope(
      app2_scope, web_app::WebAppFilter::InstalledInChrome());
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, app2_id);

  app3_match = registrar().FindBestAppWithUrlInScope(
      app3_scope, web_app::WebAppFilter::InstalledInChrome());
  DCHECK(app3_match);
  EXPECT_EQ(*app3_match, app3_id);

  // Apps in the process of uninstalling are ignored.
  app4_match = registrar().FindBestAppWithUrlInScope(
      app4_scope,
      web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  EXPECT_FALSE(app4_match);
}

TEST_F(WebAppRegistrarTest, FindPwaBasedOnStartUrlIfScopeIsEmpty) {
  StartWebAppProvider();

  const GURL app1_launch("https://example.com/app/specific/launch1");

  const GURL app2_scope("https://example.com/app");
  const GURL app2_page("https://example.com/app/specific/page2");

  const GURL app3_launch("https://example.com/app/specific/launch3");
  const webapps::AppId app3_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, app3_launch);

  auto app1 = test::CreateWebApp(app1_launch);
  RegisterAppUnsafe(std::move(app1));

  auto app2 = test::CreateWebApp(app2_scope);
  app2->SetScope(app2_scope);
  RegisterAppUnsafe(std::move(app2));

  auto app3 = test::CreateWebApp(app3_launch);
  RegisterAppUnsafe(std::move(app3));

  std::optional<webapps::AppId> app2_match =
      registrar().FindBestAppWithUrlInScope(
          app2_page, web_app::WebAppFilter::InstalledInChrome());
  ASSERT_TRUE(app2_match);
  EXPECT_EQ(app2_match, std::optional<webapps::AppId>(app3_id));
}

TEST_F(WebAppRegistrarTest, BeginAndCommitUpdate) {
  base::flat_set<webapps::AppId> ids =
      PopulateRegistryWithApps("https://example.com/path", 10);
  StartWebAppProvider();

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate(future.GetCallback());

    for (auto& app_id : ids) {
      WebApp* app = update->UpdateApp(app_id);
      EXPECT_TRUE(app);
      app->SetName("New Name");
    }

    // Acquire each app second time to make sure update requests get merged.
    for (auto& app_id : ids) {
      WebApp* app = update->UpdateApp(app_id);
      EXPECT_TRUE(app);
      app->SetDisplayMode(DisplayMode::kStandalone);
    }
  }
  EXPECT_TRUE(future.Take());

  // Make sure that all app ids were written to the database.
  auto registry_written = database_factory().ReadRegistry();
  EXPECT_EQ(ids.size(), registry_written.size());

  for (auto& kv : registry_written) {
    EXPECT_EQ("New Name", kv.second->untranslated_name());
    ids.erase(kv.second->app_id());
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CommitEmptyUpdate) {
  base::flat_set<webapps::AppId> ids =
      PopulateRegistryWithApps("https://example.com/path", 10);
  StartWebAppProvider();
  const auto initial_registry = database_factory().ReadRegistry();

  {
    base::test::TestFuture<bool> future;
    {
      ScopedRegistryUpdate update =
          sync_bridge().BeginUpdate(future.GetCallback());
    }
    EXPECT_TRUE(future.Take());

    auto registry = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(initial_registry, registry));
  }

  {
    base::test::TestFuture<bool> future;
    {
      ScopedRegistryUpdate update =
          sync_bridge().BeginUpdate(future.GetCallback());

      WebApp* app = update->UpdateApp("unknown");
      EXPECT_FALSE(app);
    }
    EXPECT_TRUE(future.Take());

    auto registry = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(initial_registry, registry));
  }
}

TEST_F(WebAppRegistrarTest, ScopedRegistryUpdate) {
  base::flat_set<webapps::AppId> ids =
      PopulateRegistryWithApps("https://example.com/path", 10);
  StartWebAppProvider();
  const auto initial_registry = database_factory().ReadRegistry();

  // Test empty update first.
  { ScopedRegistryUpdate update = sync_bridge().BeginUpdate(); }
  EXPECT_TRUE(
      IsRegistryEqual(initial_registry, database_factory().ReadRegistry()));

  {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();

    for (auto& app_id : ids) {
      WebApp* app = update->UpdateApp(app_id);
      EXPECT_TRUE(app);
      app->SetDescription("New Description");
    }
  }

  // Make sure that all app ids were written to the database.
  auto updated_registry = database_factory().ReadRegistry();
  EXPECT_EQ(ids.size(), updated_registry.size());

  for (auto& kv : updated_registry) {
    EXPECT_EQ(kv.second->untranslated_description(), "New Description");
    ids.erase(kv.second->app_id());
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CopyOnWrite) {
  StartWebAppProvider();

  const GURL start_url("https://example.com");
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  const WebApp* app = nullptr;
  {
    auto new_app = test::CreateWebApp(start_url);
    app = new_app.get();
    RegisterAppUnsafe(std::move(new_app));
  }

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    WebApp* app_copy = update->UpdateApp(app_id);
    EXPECT_TRUE(app_copy);
    EXPECT_NE(app_copy, app);

    app_copy->SetName("New Name");
    EXPECT_EQ(app_copy->untranslated_name(), "New Name");
    EXPECT_EQ(app->untranslated_name(), "Name");

    app_copy->AddSource(WebAppManagement::kPolicy);
    app_copy->RemoveSource(WebAppManagement::kSync);

    EXPECT_FALSE(app_copy->IsSynced());
    EXPECT_TRUE(app_copy->HasAnySources());

    EXPECT_TRUE(app->IsSynced());
    EXPECT_TRUE(app->HasAnySources());
  }
  EXPECT_TRUE(future.Take());

  // Pointer value stays the same.
  EXPECT_EQ(app, registrar().GetAppById(app_id));

  EXPECT_EQ(app->untranslated_name(), "New Name");
  EXPECT_FALSE(app->IsSynced());
  EXPECT_TRUE(app->HasAnySources());
}

TEST_F(WebAppRegistrarTest, CountUserInstalledApps) {
  StartWebAppProvider();

  const std::string base_url{"https://example.com/path"};

  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    int i = static_cast<int>(type);
    auto web_app =
        test::CreateWebApp(GURL(base_url + base::NumberToString(i)), type);
    RegisterAppUnsafe(std::move(web_app));
  }

  // User-installed apps have one of the following types:
  // - `WebAppManagement::kSync`
  // - `WebAppManagement::kUserInstalled`
  // - `WebAppManagement::kWebAppStore`
  // - `WebAppManagement::kOneDriveIntegration`
  // - `WebAppManagement::kIwaUserInstalled`
  EXPECT_EQ(5, registrar().CountUserInstalledApps());
}

TEST_F(WebAppRegistrarTest, CountUserInstalledAppsDiy) {
  StartWebAppProvider();

  int i = 1;
  const std::string base_url{"https://example.com/path"};

  // Sync installed non-DIY.
  auto web_app1 = test::CreateWebApp(GURL(base_url + base::NumberToString(i)),
                                     WebAppManagement::kSync);
  web_app1->SetIsDiyApp(/*is_diy_app=*/false);
  RegisterAppUnsafe(std::move(web_app1));
  ++i;

  // Sync installed DIY.
  auto web_app2 = test::CreateWebApp(GURL(base_url + base::NumberToString(i)),
                                     WebAppManagement::kSync);
  web_app2->SetIsDiyApp(/*is_diy_app=*/true);
  RegisterAppUnsafe(std::move(web_app2));
  ++i;

  // Policy installed DIY (not counted as part of user installed)
  auto web_app3 = test::CreateWebApp(GURL(base_url + base::NumberToString(i)),
                                     WebAppManagement::kPolicy);
  web_app3->SetIsDiyApp(/*is_diy_app=*/true);
  RegisterAppUnsafe(std::move(web_app3));
  ++i;

  EXPECT_EQ(2, registrar().CountUserInstalledApps());
  EXPECT_EQ(1, registrar().CountUserInstalledDiyApps());
}

TEST_F(WebAppRegistrarTest, GetAllIsolatedWebAppStoragePartitionConfigs) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  constexpr char kIwaHostname[] =
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
  constexpr char kExpectedIwaStoragePartitionDomain[] =
      "i1kr80qqyjuuVC4UFPN7ovBngVoA2HbXGtTXtmQn6/H4=";
  GURL start_url(base::StrCat({webapps::kIsolatedAppScheme,
                               url::kStandardSchemeSeparator, kIwaHostname}));
  auto isolated_web_app = test::CreateWebApp(start_url);
  const webapps::AppId app_id = isolated_web_app->app_id();

  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());
  RegisterAppUnsafe(std::move(isolated_web_app));

  std::vector<content::StoragePartitionConfig> storage_partition_configs =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(app_id);

  auto expected_config = content::StoragePartitionConfig::Create(
      profile(), kExpectedIwaStoragePartitionDomain,
      /*partition_name=*/"", /*in_memory=*/false);
  ASSERT_EQ(1UL, storage_partition_configs.size());
  EXPECT_EQ(expected_config, storage_partition_configs[0]);
}

TEST_F(
    WebAppRegistrarTest,
    GetAllIsolatedWebAppStoragePartitionConfigsEmptyWhenNotLocallyInstalled) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  GURL start_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  auto isolated_web_app = test::CreateWebApp(start_url);
  const webapps::AppId app_id = isolated_web_app->app_id();

  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());
  isolated_web_app->SetInstallState(
      proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
  RegisterAppUnsafe(std::move(isolated_web_app));

  std::vector<content::StoragePartitionConfig> storage_partition_configs =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(app_id);

  EXPECT_TRUE(storage_partition_configs.empty());
}

TEST_F(WebAppRegistrarTest, SaveAndGetInMemoryControlledFramePartitionConfig) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  constexpr char kIwaHostname[] =
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
  constexpr char kExpectedIwaStoragePartitionDomain[] =
      "i1kr80qqyjuuVC4UFPN7ovBngVoA2HbXGtTXtmQn6/H4=";
  GURL start_url(base::StrCat({webapps::kIsolatedAppScheme,
                               url::kStandardSchemeSeparator, kIwaHostname}));
  auto isolated_web_app = test::CreateWebApp(start_url);
  const webapps::AppId app_id = isolated_web_app->app_id();
  auto url_info = IsolatedWebAppUrlInfo::Create(start_url);
  ASSERT_TRUE(url_info.has_value());

  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());
  RegisterAppUnsafe(std::move(isolated_web_app));

  auto output_config =
      registrar().SaveAndGetInMemoryControlledFramePartitionConfig(
          url_info.value(), "partition_1");

  auto expected_config_cf_1 = content::StoragePartitionConfig::Create(
      profile(), kExpectedIwaStoragePartitionDomain,
      /*partition_name=*/"partition_1", /*in_memory=*/true);

  EXPECT_EQ(expected_config_cf_1, output_config);
}

TEST_F(WebAppRegistrarTest,
       AppsFromSyncAndPendingInstallationExcludedFromGetAppIds) {
  PopulateRegistryWithApps("https://example.com/path/", 20);
  StartWebAppProvider();

  EXPECT_EQ(20u, registrar().GetAppIds().size());

  std::unique_ptr<WebApp> web_app_in_sync_install =
      test::CreateWebApp(GURL("https://example.org/"));
  web_app_in_sync_install->SetIsFromSyncAndPendingInstallation(true);

  const webapps::AppId web_app_in_sync_install_id =
      web_app_in_sync_install->app_id();
  RegisterAppUnsafe(std::move(web_app_in_sync_install));

  // Tests that GetAppIds() excludes web app in sync install:
  std::vector<webapps::AppId> ids = registrar().GetAppIds();
  EXPECT_EQ(20u, ids.size());
  for (const webapps::AppId& app_id : ids) {
    EXPECT_NE(app_id, web_app_in_sync_install_id);
  }

  // Tests that GetAppsIncludingStubs() returns a web app which is either in
  // GetAppIds() set or it is the web app in sync install:
  bool web_app_in_sync_install_found = false;
  for (const WebApp& web_app : registrar().GetAppsIncludingStubs()) {
    if (web_app.app_id() == web_app_in_sync_install_id) {
      web_app_in_sync_install_found = true;
    } else {
      EXPECT_TRUE(base::Contains(ids, web_app.app_id()));
    }
  }
  EXPECT_TRUE(web_app_in_sync_install_found);
}

TEST_F(WebAppRegistrarTest, NotLocallyInstalledAppGetsDisplayModeBrowser) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().InstallAppLocally(app_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       NotLocallyInstalledAppGetsDisplayModeBrowserEvenForIsolatedWebApps) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  std::unique_ptr<WebApp> web_app =
      test::CreateWebApp(GURL("isolated-app://random_name"));
  const webapps::AppId app_id = web_app->app_id();
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
  web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());

  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       IsolatedWebAppsGetDisplayModeStandaloneRegardlessOfUserSettings) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  std::unique_ptr<WebApp> web_app =
      test::CreateWebApp(GURL("isolated-app://random_name"));
  const webapps::AppId app_id = web_app->app_id();

  // Valid manifest must have standalone display mode
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  web_app->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
  web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());

  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       IsolatedWebAppsGetDisplayModeBorderlessRegardlessOfUserSettings) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  std::unique_ptr<WebApp> web_app =
      test::CreateWebApp(GURL("isolated-app://random_name"));
  const webapps::AppId app_id = web_app->app_id();

  web_app->SetDisplayMode(DisplayMode::kBorderless);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  web_app->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
  web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());

  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBorderless,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest, NotLocallyInstalledAppGetsDisplayModeOverride) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  std::vector<DisplayMode> display_mode_overrides;
  display_mode_overrides.push_back(DisplayMode::kFullscreen);
  display_mode_overrides.push_back(DisplayMode::kMinimalUi);

  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetDisplayModeOverride(display_mode_overrides);
  web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().InstallAppLocally(app_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(DisplayMode::kMinimalUi,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       CheckDisplayOverrideFromGetEffectiveDisplayModeFromManifest) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  std::vector<DisplayMode> display_mode_overrides;
  display_mode_overrides.push_back(DisplayMode::kFullscreen);
  display_mode_overrides.push_back(DisplayMode::kMinimalUi);

  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetDisplayModeOverride(display_mode_overrides);
  web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(DisplayMode::kFullscreen,
            registrar().GetEffectiveDisplayModeFromManifest(app_id));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().InstallAppLocally(app_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(DisplayMode::kFullscreen,
            registrar().GetEffectiveDisplayModeFromManifest(app_id));
}

TEST_F(WebAppRegistrarTest, WindowControlsOverlay) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp();
  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(false, registrar().GetWindowControlsOverlayEnabled(app_id));

  sync_bridge().SetAppWindowControlsOverlayEnabled(app_id, true);
  EXPECT_EQ(true, registrar().GetWindowControlsOverlayEnabled(app_id));

  sync_bridge().SetAppWindowControlsOverlayEnabled(app_id, false);
  EXPECT_EQ(false, registrar().GetWindowControlsOverlayEnabled(app_id));
}

TEST_F(WebAppRegistrarTest, IsRegisteredLaunchProtocol) {
  StartWebAppProvider();

  apps::ProtocolHandlerInfo protocol_handler_info1;
  protocol_handler_info1.protocol = "web+test";
  protocol_handler_info1.url = GURL("http://example.com/test=%s");

  apps::ProtocolHandlerInfo protocol_handler_info2;
  protocol_handler_info2.protocol = "web+test2";
  protocol_handler_info2.url = GURL("http://example.com/test2=%s");

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  const webapps::AppId app_id = web_app->app_id();
  web_app->SetProtocolHandlers(
      {protocol_handler_info1, protocol_handler_info2});
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_TRUE(registrar().IsRegisteredLaunchProtocol(app_id, "web+test"));
  EXPECT_TRUE(registrar().IsRegisteredLaunchProtocol(app_id, "web+test2"));
  EXPECT_FALSE(registrar().IsRegisteredLaunchProtocol(app_id, "web+test3"));
  EXPECT_FALSE(registrar().IsRegisteredLaunchProtocol(app_id, "mailto"));
}

TEST_F(WebAppRegistrarTest, TestIsDefaultManagementInstalled) {
  StartWebAppProvider();

  auto web_app1 =
      test::CreateWebApp(GURL("https://start.com"), WebAppManagement::kDefault);
  auto web_app2 = test::CreateWebApp(GURL("https://starter.com"),
                                     WebAppManagement::kPolicy);
  const webapps::AppId app_id1 = web_app1->app_id();
  const webapps::AppId app_id2 = web_app2->app_id();
  RegisterAppUnsafe(std::move(web_app1));
  RegisterAppUnsafe(std::move(web_app2));

  // Currently default installed.
  EXPECT_TRUE(registrar().IsInstalledByDefaultManagement(app_id1));
  // Currently installed by source other than installed.
  EXPECT_FALSE(registrar().IsInstalledByDefaultManagement(app_id2));

  // Uninstalling the previously default installed app.
  Uninstall(app_id1);
  EXPECT_FALSE(registrar().IsInstalledByDefaultManagement(app_id1));
}

// This test uses SetLinkCapturingUserPreference, which is not appropriate for
// ChromeOS because link capturing preferences & overlapping scopes have custom
// behavior on CrOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(WebAppRegistrarTest, AppsDoNotOverlapIfNestedScope) {
  StartWebAppProvider();

  // Initialize 2 apps, with app2 having a scope with the same origin as app1
  // but is nested. If app1 is passed as an input, then app2 is not returned as
  // an overlapping app since nested scopes are not considered overlapping.
  auto web_app1 =
      test::CreateWebApp(GURL("https://example.com"), WebAppManagement::kSync);
  web_app1->SetScope(GURL("https://example_scope.com"));

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/def"),
                                     WebAppManagement::kDefault);
  web_app2->SetScope(GURL("https://example_scope.com/nested"));
  web_app2->SetLinkCapturingUserPreference(
      proto::NAVIGATION_CAPTURING_PREFERENCE_CAPTURE);

  const webapps::AppId app_id1 = web_app1->app_id();
  const webapps::AppId app_id2 = web_app2->app_id();
  RegisterAppUnsafe(std::move(web_app1));
  RegisterAppUnsafe(std::move(web_app2));

  EXPECT_TRUE(registrar().GetOverlappingAppsMatchingScope(app_id1).empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

using WebAppRegistrarTest_ScopeExtensions = WebAppRegistrarTest;

TEST_F(WebAppRegistrarTest_ScopeExtensions, IsUrlInAppExtendedScope) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp(GURL("https://example.com/start"));
  webapps::AppId app_id = web_app->app_id();

  // Limit scope to https://example.app/extended_scope
  auto associate_url = GURL("https://example.app");
  auto associate_extended_scope = GURL(associate_url.spec() + "extended_scope");

  // Manifest entry {"origin": "https://*.example.co"}.
  auto associate_url2 = GURL("https://example.co");

  // Full scope
  auto associate_url3 = GURL("https://example.co.uk");

  // Scope with query and fragment
  auto associate_url4 =
      GURL("https://example.com.jp/search?q=asdf+text#fragment");
  auto associate_extended_scope4 = GURL(associate_url4.spec());

  auto associate_url5 = GURL("https://example.com/index.html/#fragment");
  auto associate_extended_scope5 = GURL(associate_url5.spec());

  web_app->SetValidatedScopeExtensions(
      {ScopeExtensionInfo::CreateForScope(associate_extended_scope),
       ScopeExtensionInfo::CreateForScope(associate_url2,
                                          /*has_origin_wildcard=*/true),
       ScopeExtensionInfo::CreateForScope(associate_url3),
       ScopeExtensionInfo::CreateForScope(associate_extended_scope4),
       ScopeExtensionInfo::CreateForScope(associate_extended_scope5)});
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(
      registrar().GetAppExtendedScopeScore(GURL("https://test.com"), app_id),
      0);

  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.com/path"), app_id),
            0);

  // Scope is extended to all sub-domains of example.co with the wildcard
  // prefix.
  EXPECT_GT(registrar().GetAppExtendedScopeScore(GURL("https://app.example.co"),
                                                 app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://test.app.example.co"), app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.co/path"), app_id),
            0);

  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.app/extended_scope"), app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(GURL("https://example.co.uk"),
                                                 app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.com.jp/search"), app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.com.jp/search?q=something"), app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.com/index.html"), app_id),
            0);
  EXPECT_GT(registrar().GetAppExtendedScopeScore(
                GURL("https://example.com/index.html#asdfragment"), app_id),
            0);

  // Scope is extended to the example.app domain but not to the sub-domain
  // test.example.app as there was no wildcard prefix.
  EXPECT_EQ(registrar().GetAppExtendedScopeScore(
                GURL("https://test.example.app"), app_id),
            0);

  EXPECT_EQ(registrar().GetAppExtendedScopeScore(
                GURL("https://other.origin.com"), app_id),
            0);
  EXPECT_EQ(registrar().GetAppExtendedScopeScore(GURL("https://testexample.co"),
                                                 app_id),
            0);
  EXPECT_EQ(registrar().GetAppExtendedScopeScore(
                GURL("https://app.example.com"), app_id),
            0);
}

TEST_F(WebAppRegistrarTest_TabStrip, TabbedAppNewTabUrl) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  webapps::AppId app_id = web_app->app_id();
  GURL new_tab_url = GURL("https://example.com/path/newtab");

  blink::Manifest::NewTabButtonParams new_tab_button_params;
  new_tab_button_params.url = new_tab_url;
  TabStrip tab_strip;
  tab_strip.new_tab_button = new_tab_button_params;

  web_app->SetDisplayMode(DisplayMode::kTabbed);
  web_app->SetTabStrip(tab_strip);
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(registrar().GetAppNewTabUrl(app_id), new_tab_url);
}

TEST_F(WebAppRegistrarTest_TabStrip, TabbedAppAutoNewTabUrl) {
  StartWebAppProvider();

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  webapps::AppId app_id = web_app->app_id();

  web_app->SetDisplayMode(DisplayMode::kTabbed);
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_EQ(registrar().GetAppNewTabUrl(app_id),
            registrar().GetAppStartUrl(app_id));
}

TEST_F(WebAppRegistrarTest, VerifyPlaceholderFinderBehavior) {
  // Please note, this is a bad state done to test crbug.com/1427340.
  // This should not occur once crbug.com/1434692 is implemented.
  StartWebAppProvider();

  // Add first app with install_url in the registry as a non-placeholder app,
  // verify that the app is not a placeholder.
  GURL install_url("https://start_install.com/");
  auto web_app1 =
      test::CreateWebApp(GURL("https://start1.com"), WebAppManagement::kPolicy);
  const webapps::AppId app_id1 = web_app1->app_id();
  RegisterAppUnsafe(std::move(web_app1));
  test::AddInstallUrlAndPlaceholderData(
      profile()->GetPrefs(), &sync_bridge(), app_id1, install_url,
      ExternalInstallSource::kExternalPolicy, /*is_placeholder=*/false);
  EXPECT_FALSE(
      registrar()
          .LookupPlaceholderAppId(install_url, WebAppManagement::kPolicy)
          .has_value());

  // Add second app with same install_url in the registrar as a placeholder,
  // verify that app shows up as a placeholder.
  auto web_app2 =
      test::CreateWebApp(GURL("https://start2.com"), WebAppManagement::kPolicy);
  const webapps::AppId app_id2 = web_app2->app_id();
  RegisterAppUnsafe(std::move(web_app2));
  test::AddInstallUrlAndPlaceholderData(
      profile()->GetPrefs(), &sync_bridge(), app_id2, install_url,
      ExternalInstallSource::kExternalPolicy, /*is_placeholder=*/true);
  auto placeholder_id = registrar().LookupPlaceholderAppId(
      install_url, WebAppManagement::kPolicy);

  // This will fail if the fix for crbug.com/1427340 is reverted.
  EXPECT_TRUE(placeholder_id.has_value());
  EXPECT_EQ(placeholder_id.value(), app_id2);
}

TEST_F(WebAppRegistrarTest, InnerAndOuterScopeIntentPicker) {
  StartWebAppProvider();
  const GURL document_url("https://abc.com/inner/abc.html");

  auto outer_web_app =
      test::CreateWebApp(GURL("https://abc.com"), WebAppManagement::kPolicy);
  outer_web_app->SetName("ABC_Outer");
  outer_web_app->SetScope(GURL("https://abc.com/"));
  outer_web_app->SetInstallState(
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  const webapps::AppId outer_app_id = outer_web_app->app_id();
  RegisterAppUnsafe(std::move(outer_web_app));

  auto inner_web_app = test::CreateWebApp(GURL("https://abc.com/inner"),
                                          WebAppManagement::kDefault);
  inner_web_app->SetName("ABC_Inner");
  inner_web_app->SetScope(GURL("https://abc.com/inner"));
  inner_web_app->SetInstallState(
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  const webapps::AppId inner_app_id = inner_web_app->app_id();
  RegisterAppUnsafe(std::move(inner_web_app));

  // This should not be considered since the scopes do not match the visited
  // URL.
  auto no_match_scope_app =
      test::CreateWebApp(GURL("https://def.com/"), WebAppManagement::kSync);
  no_match_scope_app->SetName("App_No_Match");
  no_match_scope_app->SetScope(GURL("https://def.com/"));
  const webapps::AppId no_match_scope_app_id = no_match_scope_app->app_id();
  RegisterAppUnsafe(std::move(no_match_scope_app));

  // This should not be considered since this app is not set to open in a new
  // window.
  auto browser_mode_app = test::CreateWebApp(
      GURL("https://abc.com/inner/outer"), WebAppManagement::kSync);
  browser_mode_app->SetName("App_Browser");
  browser_mode_app->SetScope(GURL("https://abc.com/inner/"));
  browser_mode_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  const webapps::AppId browser_mode_app_id = browser_mode_app->app_id();
  RegisterAppUnsafe(std::move(browser_mode_app));

  EXPECT_THAT(registrar().GetAllAppsControllingUrl(document_url),
              ElementsAre(Pair(inner_app_id, "ABC_Inner"),
                          Pair(outer_app_id, "ABC_Outer")));
}

TEST_F(WebAppRegistrarTest, GetAllAppsControllingUrl_ScopeExtensions) {
  base::test::ScopedFeatureList feature_list(
      features::kPwaNavigationCapturingWithScopeExtensions);

  StartWebAppProvider();

  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/start"));
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info->scope = GURL("https://example.com/app/");
  web_app_info->validated_scope_extensions = {
      ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL("https://example.org")))};
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_info));

  const GURL url_in_scope("https://example.com/app/page.html");
  const GURL url_in_extension("https://example.org/page.html");
  const GURL url_outside("https://example.net/page.html");

  auto controlling_apps_in_scope =
      registrar().GetAllAppsControllingUrl(url_in_scope);
  EXPECT_EQ(1u, controlling_apps_in_scope.size());
  EXPECT_EQ(app_id, controlling_apps_in_scope.begin()->first);

  auto controlling_apps_in_extension =
      registrar().GetAllAppsControllingUrl(url_in_extension);
  EXPECT_EQ(1u, controlling_apps_in_extension.size());
  EXPECT_EQ(app_id, controlling_apps_in_extension.begin()->first);

  auto controlling_apps_outside =
      registrar().GetAllAppsControllingUrl(url_outside);
  EXPECT_TRUE(controlling_apps_outside.empty());
}

TEST_F(WebAppRegistrarTest, GetTrustedIconsIfPopulatedSingleNoSize) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  apps::IconInfo trusted_icon;
  trusted_icon.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon.square_size_px = 128;
  trusted_icon.url = GURL("https://abc.com/icon.jpg");
  web_app->SetTrustedIcons({trusted_icon});
  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(trusted_icon));
  EXPECT_EQ(trusted_icon,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/512));
}

TEST_F(WebAppRegistrarTest, EmptyTrustedOrManifestIcons) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  // Explicitly ensure that there are no manifest or trusted icons.
  web_app->SetManifestIcons({});
  web_app->SetTrustedIcons({});
  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id), IsEmpty());
  EXPECT_EQ(std::nullopt,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/128));
}

TEST_F(WebAppRegistrarTest, NoTrustedIconsFallbackToManifest) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  // Explicitly ensure that there are no trusted icons, but manifest icons are
  // populated.
  web_app->SetTrustedIcons({});

  apps::IconInfo manifest_icon;
  manifest_icon.purpose = apps::IconInfo::Purpose::kAny;
  manifest_icon.square_size_px = 128;
  manifest_icon.url = GURL("https://abc.com/icon.jpg");
  web_app->SetManifestIcons({manifest_icon});

  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  // There are no trusted app icons, but `manifest_icon` is used as the
  // fallback.
  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(manifest_icon));
  EXPECT_EQ(manifest_icon,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/64));
}

TEST_F(WebAppRegistrarTest, NoTrustedIconsFallbackToManifestMultipleIcons) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  // Explicitly ensure that there are no trusted icons, but manifest icons are
  // populated.
  web_app->SetTrustedIcons({});

  apps::IconInfo manifest_icon1;
  manifest_icon1.purpose = apps::IconInfo::Purpose::kAny;
  manifest_icon1.square_size_px = 128;
  manifest_icon1.url = GURL("https://abc.com/icon.jpg");
  apps::IconInfo manifest_icon2;
  manifest_icon2.purpose = apps::IconInfo::Purpose::kMaskable;
  manifest_icon2.square_size_px = 256;
  manifest_icon2.url = GURL("https://abc.com/icon2.jpg");

  web_app->SetManifestIcons({manifest_icon1, manifest_icon2});

  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  // There are no trusted app icons, but `manifest_icon2` is used as the
  // fallback, since that is closest to the input_size.
  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(manifest_icon1, manifest_icon2));
  EXPECT_EQ(manifest_icon2,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/512));
}

TEST_F(WebAppRegistrarTest, MultipleTrustedIconsUseBiggestClosestToSize) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  web_app->SetManifestIcons({});

  apps::IconInfo trusted_icon1;
  trusted_icon1.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon1.square_size_px = 256;
  trusted_icon1.url = GURL("https://abc.com/icon.jpg");
  apps::IconInfo trusted_icon2;
  trusted_icon2.purpose = apps::IconInfo::Purpose::kMaskable;
  trusted_icon2.square_size_px = 512;
  trusted_icon2.url = GURL("https://abc.com/icon2.jpg");
  apps::IconInfo trusted_icon3;
  trusted_icon3.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon3.square_size_px = 96;
  trusted_icon3.url = GURL("https://abc.com/icon3.jpg");
  web_app->SetTrustedIcons({trusted_icon1, trusted_icon2, trusted_icon3});

  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(trusted_icon1, trusted_icon2, trusted_icon3));
  // `trusted_icon1` is used, since it is the biggest icon with size closer to
  // the input size.
  EXPECT_EQ(trusted_icon1,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/128));
}

TEST_F(WebAppRegistrarTest, MultipleTrustedIconsUseSmallerCloserToSize) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  web_app->SetManifestIcons({});

  apps::IconInfo trusted_icon1;
  trusted_icon1.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon1.square_size_px = 128;
  trusted_icon1.url = GURL("https://abc.com/icon.jpg");
  apps::IconInfo trusted_icon2;
  trusted_icon2.purpose = apps::IconInfo::Purpose::kMaskable;
  trusted_icon2.square_size_px = 256;
  trusted_icon2.url = GURL("https://abc.com/icon2.jpg");
  apps::IconInfo trusted_icon3;
  trusted_icon3.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon3.square_size_px = 96;
  trusted_icon3.url = GURL("https://abc.com/icon3.jpg");
  web_app->SetTrustedIcons({trusted_icon1, trusted_icon2, trusted_icon3});

  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(trusted_icon1, trusted_icon2, trusted_icon3));
  // `trusted_icon2` is used, since it is the largest icon with size closer to
  // the input size but smaller than the input size.
  EXPECT_EQ(trusted_icon2,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/512));
}

TEST_F(WebAppRegistrarTest, AllIconSizesHigherThanInputSize) {
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  web_app->SetManifestIcons({});

  apps::IconInfo trusted_icon1;
  trusted_icon1.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon1.square_size_px = 128;
  trusted_icon1.url = GURL("https://abc.com/icon.jpg");
  apps::IconInfo trusted_icon2;
  trusted_icon2.purpose = apps::IconInfo::Purpose::kMaskable;
  trusted_icon2.square_size_px = 256;
  trusted_icon2.url = GURL("https://abc.com/icon2.jpg");
  apps::IconInfo trusted_icon3;
  trusted_icon3.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon3.square_size_px = 96;
  trusted_icon3.url = GURL("https://abc.com/icon3.jpg");
  web_app->SetTrustedIcons({trusted_icon1, trusted_icon2, trusted_icon3});

  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(trusted_icon1, trusted_icon2, trusted_icon3));
  // `trusted_icon3` is used, since it is the smallest icon with size closer to
  // the input size but larger than the input size.
  EXPECT_EQ(trusted_icon3,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/64));
}

TEST_F(WebAppRegistrarTest, NoSizesProvidedNoMetadata) {
  // Crash fix for
  StartWebAppProvider();
  auto web_app = test::CreateWebApp(GURL("https://abc.com"),
                                    WebAppManagement::kUserInstalled);
  web_app->SetName("ABC");
  web_app->SetScope(GURL("https://abc.com/"));
  web_app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  web_app->SetManifestIcons({});

  apps::IconInfo trusted_icon1;
  trusted_icon1.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon1.url = GURL("https://abc.com/icon.jpg");
  apps::IconInfo trusted_icon2;
  trusted_icon2.purpose = apps::IconInfo::Purpose::kAny;
  trusted_icon2.url = GURL("https://abc.com/icon2.jpg");
  web_app->SetTrustedIcons({trusted_icon1, trusted_icon2});

  const webapps::AppId app_id = web_app->app_id();
  RegisterAppUnsafe(std::move(web_app));

  EXPECT_THAT(registrar().GetTrustedAppIconsMetadata(app_id),
              ElementsAre(trusted_icon1, trusted_icon2));
  EXPECT_EQ(std::nullopt,
            registrar().GetSingleTrustedAppIconForSecuritySurfaces(
                app_id, /*input_size=*/128));
}

TEST_F(WebAppRegistrarTest, TrustedIconMetrics) {
  base::HistogramTester histogram_tester;

  // Set up the registry with 10 apps, and set trusted icons on 5 of them.
  Registry test_registry =
      CreateRegistryForTesting("https://example.com/path", 10);
  int i = 0;
  apps::IconInfo icon_info(GURL("https://www.example.com/icon.png"),
                           icon_size::k48);
  for (auto& apps : test_registry) {
    if (i % 2 == 0) {
      apps.second->SetTrustedIcons({icon_info});
    }
    i++;
  }

  PopulateRegistry(std::move(test_registry));
  StartWebAppProvider();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.InstalledCount.HasTrustedIcons"),
      base::BucketsAre(base::Bucket(/*min=*/5,
                                    /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.InstalledCount.HasNoTrustedIcons"),
      base::BucketsAre(base::Bucket(/*min=*/5,
                                    /*count=*/1)));
}

#if BUILDFLAG(IS_CHROMEOS)

class WebAppRegistrarAshTest : public WebAppTest {
 public:
  void SetUp() override {
    // TODO(crbug.com/40275387): Consider setting up a fake user in all Ash web
    // app tests.
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    fake_user_manager->AddUser(user_manager::StubAccountId());
    fake_user_manager->UserLoggedIn(
        user_manager::StubAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(
            user_manager::StubAccountId()));
    // Need to run the WebAppTest::SetUp() after the fake user manager set up
    // so that the scoped_user_manager can be destructed in the correct order.
    WebAppTest::SetUp();
  }
  WebAppRegistrarAshTest() = default;
  ~WebAppRegistrarAshTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(WebAppRegistrarAshTest, SourceSupported) {
  const GURL example_url("https://example.com/my-app/start");
  const GURL swa_url("chrome://swa/start");
  const GURL uninstalling_url("https://example.com/uninstalling/start");

  webapps::AppId example_id;
  webapps::AppId swa_id;
  webapps::AppId uninstalling_id;
  WebAppRegistrarMutable registrar(profile());
  {
    Registry registry;

    auto example_app = test::CreateWebApp(example_url);
    example_id = example_app->app_id();
    registry.emplace(example_id, std::move(example_app));

    auto swa_app = test::CreateWebApp(swa_url, WebAppManagement::Type::kSystem);
    swa_id = swa_app->app_id();
    registry.emplace(swa_id, std::move(swa_app));

    auto uninstalling_app =
        test::CreateWebApp(uninstalling_url, WebAppManagement::Type::kSystem);
    uninstalling_app->SetIsUninstalling(true);
    uninstalling_id = uninstalling_app->app_id();
    registry.emplace(uninstalling_id, std::move(uninstalling_app));

    registrar.InitRegistry(std::move(registry));
  }

  EXPECT_EQ(registrar.CountUserInstalledApps(), 1);
  EXPECT_EQ(CountApps(registrar.GetApps()), 2);

  EXPECT_EQ(registrar.FindBestAppWithUrlInScope(
                example_url, web_app::WebAppFilter::InstalledInChrome()),
            example_id);
  EXPECT_EQ(registrar.GetAppScope(example_id),
            GURL("https://example.com/my-app/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(example_id).has_value());

  EXPECT_EQ(registrar.FindBestAppWithUrlInScope(
                swa_url, web_app::WebAppFilter::InstalledInChrome()),
            swa_id);
  EXPECT_EQ(registrar.GetAppScope(swa_id), GURL("chrome://swa/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(swa_id).has_value());

  EXPECT_FALSE(
      registrar
          .FindBestAppWithUrlInScope(uninstalling_url,
                                     web_app::WebAppFilter::InstalledInChrome())
          .has_value());
  EXPECT_EQ(registrar.GetAppScope(uninstalling_id),
            GURL("https://example.com/uninstalling/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(uninstalling_id).has_value());
  EXPECT_FALSE(base::Contains(registrar.GetAppIds(), uninstalling_id));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class WebAppRegistrarDisplayModeTest
    : public WebAppRegistrarTest,
      public testing::WithParamInterface<DisplayMode> {
 public:
  const webapps::AppId CreateAppInRegistryWithUserDisplayModeAndOverrides(
      mojom::UserDisplayMode user_display_mode,
      std::vector<DisplayMode> display_mode_overrides,
      bool is_isolated = false) {
    GURL start_url = GURL("https://example.com/start");
    if (is_isolated) {
      constexpr char kIwaHostname[] =
          "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
      start_url =
          GURL(base::StrCat({webapps::kIsolatedAppScheme,
                             url::kStandardSchemeSeparator, kIwaHostname}));
    }
    auto web_app = test::CreateWebApp(start_url);
    const webapps::AppId app_id = web_app->app_id();

    // Get the display mode from the parameterized inputs.
    web_app->SetDisplayMode(GetParam());
    web_app->SetUserDisplayMode(user_display_mode);
    web_app->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
    web_app->SetDisplayModeOverride(std::move(display_mode_overrides));

    if (is_isolated) {
      web_app->SetIsolationData(
          IsolationData::Builder(
              IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
              *IwaVersion::Create("1.0.0"))
              .Build());
    }

    RegisterAppUnsafe(std::move(web_app));
    return app_id;
  }

  // When user_display_mode indicates a user preference for opening in
  // a standalone window, we open in a minimal-ui window (for app_display_mode
  // 'browser' or 'minimal-ui') or a standalone window (for app_display_mode
  // 'standalone' or 'fullscreen'). For all other display modes, keep the
  // display modes as they're specified.
  DisplayMode GetResolvedDisplayModeForStandaloneUDM() {
    switch (GetParam()) {
      case DisplayMode::kBrowser:
      case DisplayMode::kMinimalUi:
        return DisplayMode::kMinimalUi;
      case DisplayMode::kStandalone:
      case DisplayMode::kFullscreen:
        return DisplayMode::kStandalone;
      case DisplayMode::kBorderless:
        return DisplayMode::kBorderless;
      case DisplayMode::kWindowControlsOverlay:
        return DisplayMode::kWindowControlsOverlay;
      case DisplayMode::kTabbed:
        return DisplayMode::kTabbed;
      case DisplayMode::kUndefined:
      case DisplayMode::kPictureInPicture:
        NOTREACHED();
    }
  }

  // Same as `GetResolvedDisplayModeForStandaloneUDM()`, except minimal-ui does
  // not exist for IWAs.
  DisplayMode GetResolvedDisplayModeForStandaloneUDMIsolated() {
    switch (GetParam()) {
      case DisplayMode::kBrowser:
      case DisplayMode::kMinimalUi:
      case DisplayMode::kStandalone:
      case DisplayMode::kFullscreen:
      case DisplayMode::kTabbed:
        return DisplayMode::kStandalone;
      case DisplayMode::kBorderless:
        return DisplayMode::kBorderless;
      case DisplayMode::kWindowControlsOverlay:
        return DisplayMode::kWindowControlsOverlay;
      case DisplayMode::kUndefined:
      case DisplayMode::kPictureInPicture:
        NOTREACHED();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kIsolatedWebApps};
};

// When user_display_mode indicates a user preference for opening in a browser
// tab, we open in a browser tab.
TEST_P(WebAppRegistrarDisplayModeTest, UserWantsBrowser) {
  StartWebAppProvider();
  const webapps::AppId app_id =
      CreateAppInRegistryWithUserDisplayModeAndOverrides(
          mojom::UserDisplayMode::kBrowser, {});
  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_P(WebAppRegistrarDisplayModeTest, UserWantsStandalone) {
  if (GetParam() == DisplayMode::kPictureInPicture) {
    GTEST_SKIP()
        << "PictureInPicture not supported for standalone display modes";
  }
  StartWebAppProvider();
  const webapps::AppId app_id =
      CreateAppInRegistryWithUserDisplayModeAndOverrides(
          mojom::UserDisplayMode::kStandalone, {});
  EXPECT_EQ(GetResolvedDisplayModeForStandaloneUDM(),
            registrar().GetAppEffectiveDisplayMode(app_id));
}

// When user_display_mode indicates a user preference for opening in a browser
// tab, we open in a browser tab, even if display_overrides are specified.
TEST_P(WebAppRegistrarDisplayModeTest, UserWantsBrowserStandaloneOverride) {
  StartWebAppProvider();
  const webapps::AppId app_id =
      CreateAppInRegistryWithUserDisplayModeAndOverrides(
          mojom::UserDisplayMode::kBrowser, {DisplayMode::kStandalone});
  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

// When user_display_mode indicates a user preference for opening in
// a standalone window, and the only display modes provided for
// display_overrides contain only 'fullscreen' or 'browser',  open in a
// minimal-ui window (for app_display_mode 'browser' or 'minimal-ui') or a
// standalone window (for app_display_mode 'standalone' or 'fullscreen').
TEST_P(WebAppRegistrarDisplayModeTest, UserWantsStandaloneFullScreenOverride) {
  if (GetParam() == DisplayMode::kPictureInPicture) {
    GTEST_SKIP()
        << "PictureInPicture not supported for standalone display modes";
  }
  StartWebAppProvider();
  const webapps::AppId app_id =
      CreateAppInRegistryWithUserDisplayModeAndOverrides(
          mojom::UserDisplayMode::kStandalone, {DisplayMode::kFullscreen});
  EXPECT_EQ(GetResolvedDisplayModeForStandaloneUDM(),
            registrar().GetAppEffectiveDisplayMode(app_id));
}

// When user_display_mode indicates a user preference for opening in
// a standalone window, and return the first entry that is either
// 'standalone' or 'minimal-ui' in display_override.
TEST_P(WebAppRegistrarDisplayModeTest, UserWantsStandaloneMultipleOverrides) {
  if (GetParam() == DisplayMode::kPictureInPicture) {
    GTEST_SKIP()
        << "PictureInPicture not supported for standalone display modes";
  }
  StartWebAppProvider();
  const webapps::AppId app_id =
      CreateAppInRegistryWithUserDisplayModeAndOverrides(
          mojom::UserDisplayMode::kStandalone,
          {DisplayMode::kFullscreen, DisplayMode::kBrowser,
           DisplayMode::kStandalone});
  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_P(WebAppRegistrarDisplayModeTest, UserStandaloneNoOverrideIsolated) {
  if (GetParam() == DisplayMode::kPictureInPicture) {
    GTEST_SKIP()
        << "PictureInPicture not supported for standalone display modes";
  }
  StartWebAppProvider();
  const webapps::AppId app_id =
      CreateAppInRegistryWithUserDisplayModeAndOverrides(
          mojom::UserDisplayMode::kStandalone, {}, /*is_isolated=*/true);
  EXPECT_EQ(GetResolvedDisplayModeForStandaloneUDMIsolated(),
            registrar().GetAppEffectiveDisplayMode(app_id));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppRegistrarDisplayModeTest,
                         testing::Values(DisplayMode::kBrowser,
                                         DisplayMode::kMinimalUi,
                                         DisplayMode::kStandalone,
                                         DisplayMode::kFullscreen,
                                         DisplayMode::kBorderless,
                                         DisplayMode::kPictureInPicture,
                                         DisplayMode::kWindowControlsOverlay,
                                         DisplayMode::kTabbed),
                         [](const testing::TestParamInfo<DisplayMode>& info) {
                           switch (info.param) {
                             case DisplayMode::kBrowser:
                               return "Browser";
                             case DisplayMode::kMinimalUi:
                               return "MinimalUi";
                             case DisplayMode::kStandalone:
                               return "Standalone";
                             case DisplayMode::kFullscreen:
                               return "Fullscreen";
                             case DisplayMode::kBorderless:
                               return "Borderless";
                             case DisplayMode::kPictureInPicture:
                               return "PictureInPicture";
                             case DisplayMode::kWindowControlsOverlay:
                               return "WindowControlsOverlay";
                             case DisplayMode::kTabbed:
                               return "Tabbed";
                             case DisplayMode::kUndefined:
                               NOTREACHED();
                           }
                         });


class WebAppRegistrarParameterizedTest
    : public WebAppRegistrarTest,
      public testing::WithParamInterface<
          apps::test::LinkCapturingFeatureVersion> {
 public:
  WebAppRegistrarParameterizedTest() {
    link_capturing_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam()), {});
  }

 private:
  base::test::ScopedFeatureList link_capturing_feature_list_;
};

// These tests use SetLinkCapturingUserPreference, which is not appropriate for
// ChromeOS because link capturing preferences & overlapping scopes have custom
// behavior on CrOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_P(WebAppRegistrarParameterizedTest, AppsOverlapIfSharesScope) {
  StartWebAppProvider();

  // Initialize 2 apps, both having the same scope, and set the second
  // app to capture links. If app1 is passed as an input, then
  // app2 is returned as an overlapping app that matches the scope and
  // is set by the user to handle links.
  auto web_app1 =
      test::CreateWebApp(GURL("https://example.com"), WebAppManagement::kSync);
  web_app1->SetScope(GURL("https://example_scope.com"));

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/def"),
                                     WebAppManagement::kDefault);
  web_app2->SetScope(GURL("https://example_scope.com"));
  web_app2->SetLinkCapturingUserPreference(
      proto::NAVIGATION_CAPTURING_PREFERENCE_CAPTURE);

  const webapps::AppId app_id1 = web_app1->app_id();
  const webapps::AppId app_id2 = web_app2->app_id();
  RegisterAppUnsafe(std::move(web_app1));
  RegisterAppUnsafe(std::move(web_app2));

  EXPECT_THAT(registrar().GetOverlappingAppsMatchingScope(app_id1),
              ElementsAre(app_id2));
}

TEST_P(WebAppRegistrarParameterizedTest, Filter_OpensInBrowserTab) {
  StartWebAppProvider();

  GURL app_url_1 = GURL("https://example.com/1/");
  GURL app_url_2 = GURL("https://example.com/2/");
  auto web_app_1 =
      test::CreateWebApp(app_url_1, WebAppManagement::kUserInstalled);
  auto web_app_2 =
      test::CreateWebApp(app_url_2, WebAppManagement::kUserInstalled);
  const webapps::AppId app_id_browser_tab = web_app_1->app_id();
  const webapps::AppId app_id_standalone = web_app_2->app_id();

  web_app_1->SetDisplayMode(DisplayMode::kBrowser);
  web_app_1->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  web_app_1->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
  web_app_1->SetLinkCapturingUserPreference(
      proto::NAVIGATION_CAPTURING_PREFERENCE_CAPTURE);

  web_app_2->SetDisplayMode(DisplayMode::kStandalone);
  web_app_2->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app_2->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
  web_app_2->SetLinkCapturingUserPreference(
      proto::NAVIGATION_CAPTURING_PREFERENCE_CAPTURE);

  RegisterAppUnsafe(std::move(web_app_1));
  RegisterAppUnsafe(std::move(web_app_2));

  EXPECT_TRUE(registrar().AppMatches(app_id_browser_tab,
                                     WebAppFilter::OpensInBrowserTab()));
  EXPECT_FALSE(registrar().AppMatches(app_id_browser_tab,
                                      WebAppFilter::OpensInDedicatedWindow()));
  EXPECT_EQ(app_id_browser_tab,
            registrar().FindBestAppWithUrlInScope(
                app_url_1, WebAppFilter::OpensInBrowserTab()));
  EXPECT_EQ(std::nullopt,
            registrar().FindBestAppWithUrlInScope(
                app_url_1, WebAppFilter::OpensInDedicatedWindow()));

  EXPECT_TRUE(registrar().AppMatches(app_id_standalone,
                                     WebAppFilter::OpensInDedicatedWindow()));
  EXPECT_FALSE(registrar().AppMatches(app_id_standalone,
                                      WebAppFilter::OpensInBrowserTab()));
  EXPECT_EQ(app_id_standalone,
            registrar().FindBestAppWithUrlInScope(
                app_url_2, WebAppFilter::OpensInDedicatedWindow()));
  EXPECT_EQ(std::nullopt, registrar().FindBestAppWithUrlInScope(
                              app_url_2, WebAppFilter::OpensInBrowserTab()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_P(WebAppRegistrarParameterizedTest, Filter_IsIsolatedApp) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  StartWebAppProvider();

  constexpr char kIwaHostname[] =
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
  GURL app_url(base::StrCat({webapps::kIsolatedAppScheme,
                             url::kStandardSchemeSeparator, kIwaHostname}));
  auto isolated_web_app = test::CreateWebApp(app_url);
  const webapps::AppId app_id = isolated_web_app->app_id();

  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());
  isolated_web_app->SetDisplayMode(DisplayMode::kBrowser);
  isolated_web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  RegisterAppUnsafe(std::move(isolated_web_app));

  EXPECT_TRUE(registrar().AppMatches(app_id, WebAppFilter::IsIsolatedApp()));
  EXPECT_EQ(app_id, registrar().FindBestAppWithUrlInScope(
                        app_url, WebAppFilter::IsIsolatedApp()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebAppRegistrarParameterizedTest,
#if BUILDFLAG(IS_CHROMEOS)
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV1DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOff)
#else
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn)
#endif  // BUILDFLAG(IS_CHROMEOS)
        ,
    apps::test::LinkCapturingVersionToString);

}  // namespace web_app
