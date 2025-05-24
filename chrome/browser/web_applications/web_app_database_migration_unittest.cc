// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_database_metadata.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

// Creates a protobuf web app that passes the parsing checks.
proto::WebApp CreateWebAppProtoForTesting(const std::string& name,
                                          const GURL& start_url) {
  proto::WebApp web_app;
  CHECK(start_url.is_valid());
  web_app.set_name(name);
  web_app.mutable_sync_data()->set_name(name);
  web_app.mutable_sync_data()->set_start_url(start_url.spec());
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  web_app.mutable_sync_data()->set_relative_manifest_id(
      RelativeManifestIdPath(manifest_id));
  web_app.mutable_sync_data()->set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE);
  web_app.set_scope(start_url.GetWithoutFilename().spec());
  web_app.mutable_sources()->set_user_installed(true);
#if BUILDFLAG(IS_CHROMEOS)
  web_app.mutable_chromeos_data();
  web_app.mutable_sync_data()->set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE);
#endif
  web_app.set_install_state(
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  return web_app;
}

webapps::AppId GetAppIdFromWebAppProto(const proto::WebApp& web_app) {
  CHECK(web_app.has_sync_data());
  CHECK(web_app.sync_data().has_relative_manifest_id());
  CHECK(web_app.sync_data().has_start_url());
  GURL start_url = GURL(web_app.sync_data().start_url());
  CHECK(start_url.is_valid());
  return GenerateAppId(web_app.sync_data().relative_manifest_id(), start_url);
}

// Helper function to verify database state matches registrar state.
void VerifyDatabaseRegistryEqualToRegistrar(FakeWebAppProvider* provider) {
  FakeWebAppDatabaseFactory* database_factory =
      provider->GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_NE(database_factory, nullptr);

  Registry db_registry =
      database_factory->ReadRegistry(/*allow_invalid_protos=*/true);
  const Registry& registrar_registry =
      provider->registrar_unsafe().registry_for_testing();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry, db_registry));
}

using WebAppDatabaseMigrationTest = WebAppTest;

// Installs a shortcut app in the profile by emptying its scope.
TEST_F(WebAppDatabaseMigrationTest, MigrateShortcutToDiyApp) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  // Create a webapp protobuf that should be 'migrated'
  proto::WebApp web_app_no_scope =
      CreateWebAppProtoForTesting("Test App", GURL("https://www.example.com/"));
  web_app_no_scope.clear_scope();

  proto::WebApp web_app_last_installed_from_shortcut =
      CreateWebAppProtoForTesting("App Name 2", GURL("https://example2.com"));
  web_app_last_installed_from_shortcut.set_latest_install_source(
      static_cast<uint32_t>(
          webapps::WebappInstallSource::MENU_CREATE_SHORTCUT));

  // Write them to the database.
  database_factory->WriteProtos(
      {web_app_no_scope, web_app_last_installed_from_shortcut});

  // Make sure we are only on version 1
  proto::DatabaseMetadata metadata;
  metadata.set_version(1);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;
  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_THAT(histograms.GetAllSamples("WebApp.Migrations.ShortcutAppsToDiy2"),
              base::BucketsAre(base::Bucket(/*min=*/2, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration occurs.
  webapps::AppId no_scope_app_id = GetAppIdFromWebAppProto(web_app_no_scope);
  webapps::AppId last_installed_from_shortcut_app_id =
      GetAppIdFromWebAppProto(web_app_last_installed_from_shortcut);

  ASSERT_TRUE(registrar.GetAppById(no_scope_app_id));
  ASSERT_TRUE(registrar.GetAppById(last_installed_from_shortcut_app_id));

  // Both are DIY.
  EXPECT_TRUE(registrar.GetAppById(no_scope_app_id)->is_diy_app());
  EXPECT_TRUE(
      registrar.GetAppById(last_installed_from_shortcut_app_id)->is_diy_app());

  // Both are marked as migrating from shortcut.
  EXPECT_TRUE(registrar.GetAppById(no_scope_app_id)->was_shortcut_app());
  EXPECT_TRUE(registrar.GetAppById(last_installed_from_shortcut_app_id)
                  ->was_shortcut_app());

  // Both have a scope.
  EXPECT_TRUE(registrar.GetAppById(no_scope_app_id)->scope().is_valid());
  EXPECT_TRUE(registrar.GetAppById(last_installed_from_shortcut_app_id)
                  ->scope()
                  .is_valid());

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest, MigrateInstallSourceAddUserInstalled) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  // App 1: Only kSync source (should gain kUserInstalled)
  proto::WebApp web_app_sync_only =
      CreateWebAppProtoForTesting("App 1", GURL("https://app1.com/"));
  web_app_sync_only.mutable_sources()->clear_user_installed();
  web_app_sync_only.mutable_sources()->set_sync(true);

  // App 2: kSync and kUserInstalled (should remain unchanged)
  // This state should be impossible, as there was no kUserInstalled at v0.
  proto::WebApp web_app_sync_and_user =
      CreateWebAppProtoForTesting("App 2", GURL("https://app2.com/"));
  web_app_sync_and_user.mutable_sources()->set_sync(true);
  web_app_sync_and_user.mutable_sources()->set_user_installed(true);

  // App 3: Only kUserInstalled (should remain unchanged)
  // This state should be impossible, as there was no kUserInstalled at v0.
  proto::WebApp web_app_user_only =
      CreateWebAppProtoForTesting("App 3", GURL("https://app3.com/"));
  web_app_user_only.mutable_sources()->clear_sync();
  web_app_user_only.mutable_sources()->set_user_installed(true);

  // App 4: Only kPolicy (should remain unchanged)
  proto::WebApp web_app_policy_only =
      CreateWebAppProtoForTesting("App 4", GURL("https://app4.com/"));
  web_app_policy_only.mutable_sources()->clear_sync();
  web_app_policy_only.mutable_sources()->clear_user_installed();
  web_app_policy_only.mutable_sources()->set_policy(true);

  database_factory->WriteProtos({web_app_sync_only, web_app_sync_and_user,
                                 web_app_user_only, web_app_policy_only});

  // Set version to 0 to trigger the migration to version 1 (and subsequently
  // 2).
  proto::DatabaseMetadata metadata;
  metadata.set_version(0);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;

  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.InstallSourceAddUserInstalled"),
              base::BucketsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_sync_only));
  const WebApp* app2 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_sync_and_user));
  const WebApp* app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_user_only));
  const WebApp* app4 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_policy_only));

  ASSERT_TRUE(app1);
  ASSERT_TRUE(app2);
  ASSERT_TRUE(app3);
  ASSERT_TRUE(app4);

  // App 1: Should now have kSync and kUserInstalled
  EXPECT_TRUE(app1->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(app1->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_EQ(app1->GetSources().size(), 2u);

  // App 2: Should remain kSync and kUserInstalled
  EXPECT_TRUE(app2->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(app2->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_EQ(app2->GetSources().size(), 2u);

  // App 3: Should remain kUserInstalled only
  EXPECT_FALSE(app3->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(app3->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_EQ(app3->GetSources().size(), 1u);

  // App 4: Should remain kPolicy only
  EXPECT_FALSE(app4->GetSources().Has(WebAppManagement::kSync));
  EXPECT_FALSE(app4->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_TRUE(app4->GetSources().Has(WebAppManagement::kPolicy));
  EXPECT_EQ(app4->GetSources().size(), 1u);

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest,
       MigrateInstallSourceAddUserInstalled_SyncDisabled) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  // App 1: Only kSync source (should become kUserInstalled only)
  proto::WebApp web_app_sync_only =
      CreateWebAppProtoForTesting("App 1", GURL("https://app1.com/"));
  web_app_sync_only.mutable_sources()->clear_user_installed();
  web_app_sync_only.mutable_sources()->set_sync(true);

  // App 2: kSync and kUserInstalled (should become kUserInstalled only)
  proto::WebApp web_app_sync_and_user =
      CreateWebAppProtoForTesting("App 2", GURL("https://app2.com/"));
  web_app_sync_and_user.mutable_sources()->set_sync(true);
  web_app_sync_and_user.mutable_sources()->set_user_installed(true);

  // App 3: Only kUserInstalled (should remain unchanged)
  proto::WebApp web_app_user_only =
      CreateWebAppProtoForTesting("App 3", GURL("https://app3.com/"));
  web_app_user_only.mutable_sources()->clear_sync();
  web_app_user_only.mutable_sources()->set_user_installed(true);

  // App 4: Only kPolicy (should remain unchanged)
  proto::WebApp web_app_policy_only =
      CreateWebAppProtoForTesting("App 4", GURL("https://app4.com/"));
  web_app_policy_only.mutable_sources()->clear_sync();
  web_app_policy_only.mutable_sources()->clear_user_installed();
  web_app_policy_only.mutable_sources()->set_policy(true);

  database_factory->WriteProtos({web_app_sync_only, web_app_sync_and_user,
                                 web_app_user_only, web_app_policy_only});

  // Set version to 0 to trigger the migration to version 1 (and subsequently
  // 2).
  proto::DatabaseMetadata metadata;
  metadata.set_version(0);
  database_factory->WriteMetadata(metadata);

  // Explicitly disable sync.
  database_factory->set_is_syncing_apps(false);

  base::HistogramTester histograms;

  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  // Two apps should be affected, as kSync was removed from two.
  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.InstallSourceAddUserInstalled"),
              base::BucketsAre(base::Bucket(/*min=*/2, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_sync_only));
  const WebApp* app2 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_sync_and_user));
  const WebApp* app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_user_only));
  const WebApp* app4 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_policy_only));

  ASSERT_TRUE(app1);
  ASSERT_TRUE(app2);
  ASSERT_TRUE(app3);
  ASSERT_TRUE(app4);

  // App 1: Should now have only kUserInstalled (kSync removed)
  EXPECT_FALSE(app1->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(app1->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_EQ(app1->GetSources().size(), 1u);

  // App 2: Should now have only kUserInstalled (kSync removed)
  EXPECT_FALSE(app2->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(app2->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_EQ(app2->GetSources().size(), 1u);

  // App 3: Should remain kUserInstalled only
  EXPECT_FALSE(app3->GetSources().Has(WebAppManagement::kSync));
  EXPECT_TRUE(app3->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_EQ(app3->GetSources().size(), 1u);

  // App 4: Should remain kPolicy only
  EXPECT_FALSE(app4->GetSources().Has(WebAppManagement::kSync));
  EXPECT_FALSE(app4->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_TRUE(app4->GetSources().Has(WebAppManagement::kPolicy));
  EXPECT_EQ(app4->GetSources().size(), 1u);

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest,
       MigrateDefaultDisplayModeToPlatformDisplayMode) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  // App 1: Missing CrOS UDM, has Default UDM (Browser)
  proto::WebApp web_app_missing_cros =
      CreateWebAppProtoForTesting("App 1", GURL("https://app1.com/"));
  web_app_missing_cros.mutable_sync_data()->set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  web_app_missing_cros.mutable_sync_data()->clear_user_display_mode_cros();

  // App 2: Has CrOS UDM (Standalone), missing Default UDM
  proto::WebApp web_app_missing_default =
      CreateWebAppProtoForTesting("App 2", GURL("https://app2.com/"));
  web_app_missing_default.mutable_sync_data()->set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  web_app_missing_default.mutable_sync_data()
      ->clear_user_display_mode_default();

  // App 3: Missing both UDMs
  proto::WebApp web_app_missing_both =
      CreateWebAppProtoForTesting("App 3", GURL("https://app3.com/"));
  web_app_missing_both.mutable_sync_data()->clear_user_display_mode_cros();
  web_app_missing_both.mutable_sync_data()->clear_user_display_mode_default();

  // App 4: Has both UDMs (correct state, should not change)
  proto::WebApp web_app_correct =
      CreateWebAppProtoForTesting("App 4", GURL("https://app4.com/"));
  web_app_correct.mutable_sync_data()->set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  web_app_correct.mutable_sync_data()->set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);

  database_factory->WriteProtos({web_app_missing_cros, web_app_missing_default,
                                 web_app_missing_both, web_app_correct});

  // Set version to 1 to trigger the migration to version 2.
  proto::DatabaseMetadata metadata;
  metadata.set_version(1);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;
  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.DefaultDisplayModeToPlatform"),
              base::BucketsAre(base::Bucket(/*min=*/2, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_missing_cros));
  const WebApp* app2 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_missing_default));
  const WebApp* app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_missing_both));
  const WebApp* app4 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_correct));

  ASSERT_TRUE(app1);
  ASSERT_TRUE(app2);
  ASSERT_TRUE(app3);
  ASSERT_TRUE(app4);

#if BUILDFLAG(IS_CHROMEOS)
  // App 1: CrOS UDM should be populated from Default (Browser)
  EXPECT_TRUE(app1->sync_proto().has_user_display_mode_cros());
  EXPECT_EQ(app1->sync_proto().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  EXPECT_TRUE(
      app1->sync_proto().has_user_display_mode_default());  // Default remains
  EXPECT_EQ(app1->sync_proto().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);

  // App 2: Default UDM should remain absent (CrOS doesn't populate Default)
  EXPECT_TRUE(app2->sync_proto().has_user_display_mode_cros());  // CrOS remains
  EXPECT_EQ(app2->sync_proto().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  EXPECT_FALSE(app2->sync_proto().has_user_display_mode_default());

  // App 3: CrOS UDM should default to Standalone
  EXPECT_TRUE(app3->sync_proto().has_user_display_mode_cros());
  EXPECT_EQ(app3->sync_proto().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  EXPECT_FALSE(app3->sync_proto()
                   .has_user_display_mode_default());  // Default remains absent
#else  // !BUILDFLAG(IS_CHROMEOS)
  // App 1: Default UDM should remain Browser, CrOS should remain absent
  EXPECT_FALSE(app1->sync_proto().has_user_display_mode_cros());
  EXPECT_TRUE(app1->sync_proto().has_user_display_mode_default());
  EXPECT_EQ(app1->sync_proto().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);

  // App 2: Default UDM should be populated (Standalone), CrOS remains
  // Standalone
  EXPECT_TRUE(app2->sync_proto().has_user_display_mode_cros());
  EXPECT_EQ(app2->sync_proto().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  EXPECT_TRUE(app2->sync_proto().has_user_display_mode_default());
  EXPECT_EQ(app2->sync_proto().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);

  // App 3: Default UDM should default to Standalone, CrOS remains absent
  EXPECT_FALSE(app3->sync_proto().has_user_display_mode_cros());
  EXPECT_TRUE(app3->sync_proto().has_user_display_mode_default());
  EXPECT_EQ(app3->sync_proto().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
#endif

  // App 4: Should remain unchanged
  EXPECT_TRUE(app4->sync_proto().has_user_display_mode_cros());
  EXPECT_EQ(app4->sync_proto().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  EXPECT_TRUE(app4->sync_proto().has_user_display_mode_default());
  EXPECT_EQ(app4->sync_proto().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest,
       MigratePartiallyInstalledAppsToCorrectState) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  // App 1: Correctly installed with OS integration
  proto::WebApp web_app_correct =
      CreateWebAppProtoForTesting("App 1", GURL("https://app1.com/"));
  web_app_correct.set_install_state(
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  web_app_correct.mutable_current_os_integration_states()
      ->mutable_shortcut();  // Add some OS state

  // App 2: Incorrectly marked as integrated, but no OS state
  proto::WebApp web_app_incorrect =
      CreateWebAppProtoForTesting("App 2", GURL("https://app2.com/"));
  web_app_incorrect.set_install_state(
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  // No OS state added

  // App 3: Correctly installed without OS integration
  proto::WebApp web_app_no_integration =
      CreateWebAppProtoForTesting("App 3", GURL("https://app3.com/"));
  web_app_no_integration.set_install_state(
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);

  database_factory->WriteProtos(
      {web_app_correct, web_app_incorrect, web_app_no_integration});

  // Set version to 1 to trigger the migration to version 2.
  proto::DatabaseMetadata metadata;
  metadata.set_version(1);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;
  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.PartiallyInstalledAppsToCorrectState"),
              base::BucketsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_correct));
  const WebApp* app2 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_incorrect));
  const WebApp* app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(web_app_no_integration));

  ASSERT_TRUE(app1);
  ASSERT_TRUE(app2);
  ASSERT_TRUE(app3);

  // App 1: Should remain integrated
  EXPECT_EQ(app1->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  // App 2: Should be downgraded to not integrated
  EXPECT_EQ(app2->install_state(),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);

  // App 3: Should remain not integrated
  EXPECT_EQ(app3->install_state(),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest,
       MigrateDeprecatedLaunchHandlerToClientMode) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();

  // App 1: No launch handler (should remain null, no migration)
  proto::WebApp app_no_handler =
      CreateWebAppProtoForTesting("App 1", GURL("https://app1.com/"));

  // App 2: Modern client_mode already set (should not change, no migration)
  proto::WebApp app_modern =
      CreateWebAppProtoForTesting("App 2", GURL("https://app2.com/"));
  {
    proto::LaunchHandler* launch_handler = app_modern.mutable_launch_handler();
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_NAVIGATE_NEW);
  }

  // App 3: Old route_to = NEW_CLIENT
  proto::WebApp app_old_new =
      CreateWebAppProtoForTesting("App 3", GURL("https://app3.com/"));
  {
    proto::LaunchHandler* launch_handler = app_old_new.mutable_launch_handler();
    launch_handler->set_route_to(
        proto::LaunchHandler_DeprecatedRouteTo_NEW_CLIENT);
    // Default to unspecified if not explicitly set, mimicking old state.
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);
  }

  // App 4: Old route_to = EXISTING_CLIENT, navigate = ALWAYS
  proto::WebApp app_old_existing_navigate =
      CreateWebAppProtoForTesting("App 4", GURL("https://app4.com/"));
  {
    proto::LaunchHandler* launch_handler =
        app_old_existing_navigate.mutable_launch_handler();
    launch_handler->set_route_to(
        proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT);
    launch_handler->set_navigate_existing_client(
        proto::LaunchHandler_DeprecatedNavigateExistingClient_ALWAYS);
    // Default to unspecified if not explicitly set, mimicking old state.
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);
  }

  // App 5: Old route_to = EXISTING_CLIENT, navigate = NEVER (Focus)
  proto::WebApp app_old_existing_focus =
      CreateWebAppProtoForTesting("App 5", GURL("https://app5.com/"));
  {
    proto::LaunchHandler* launch_handler =
        app_old_existing_focus.mutable_launch_handler();
    launch_handler->set_route_to(
        proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT);
    launch_handler->set_navigate_existing_client(
        proto::LaunchHandler_DeprecatedNavigateExistingClient_NEVER);
    // Default to unspecified if not explicitly set, mimicking old state.
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);
  }

  // App 6: client_mode_valid_and_specified = false (should be impossible state,
  // and migrate).
  proto::WebApp app_invalid_specified =
      CreateWebAppProtoForTesting("App 6", GURL("https://app6.com/"));
  {
    proto::LaunchHandler* launch_handler =
        app_invalid_specified.mutable_launch_handler();
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_NAVIGATE_NEW);
    launch_handler->set_client_mode_valid_and_specified(false);
  }

  // App 7: client_mode = UNSPECIFIED, but old fields present (should migrate)
  proto::WebApp app_unspecified_with_old =
      CreateWebAppProtoForTesting("App 7", GURL("https://app7.com/"));
  {
    proto::LaunchHandler* launch_handler =
        app_unspecified_with_old.mutable_launch_handler();
    launch_handler->set_route_to(
        proto::LaunchHandler_DeprecatedRouteTo_NEW_CLIENT);
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);
  }

  // App 8: Modern client_mode, but with the deprecated
  // client_mode_valid_and_specified field set, so migration occurs.
  proto::WebApp app_modern_with_deprecated_field =
      CreateWebAppProtoForTesting("App 8", GURL("https://app8.com/"));
  {
    proto::LaunchHandler* launch_handler =
        app_modern_with_deprecated_field.mutable_launch_handler();
    launch_handler->set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_NAVIGATE_NEW);
    launch_handler->set_client_mode_valid_and_specified(true);
  }

  database_factory->WriteProtos(
      {app_no_handler, app_modern, app_old_new, app_old_existing_navigate,
       app_old_existing_focus, app_invalid_specified, app_unspecified_with_old,
       app_modern_with_deprecated_field});

  // Set version to 2 to trigger the migration to version 3.
  proto::DatabaseMetadata metadata;
  metadata.set_version(2);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;
  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  // Apps 3, 4, 5, 6, 7, 8 should have been migrated.
  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.DeprecatedLaunchHandlerToClientMode"),
              base::BucketsAre(base::Bucket(/*min=*/6, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* migrated_app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_no_handler));
  const WebApp* migrated_app2 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_modern));
  const WebApp* migrated_app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_old_new));
  const WebApp* migrated_app4 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_old_existing_navigate));
  const WebApp* migrated_app5 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_old_existing_focus));
  const WebApp* migrated_app6 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_invalid_specified));
  const WebApp* migrated_app7 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_unspecified_with_old));
  const WebApp* migrated_app8 = registrar.GetAppById(
      GetAppIdFromWebAppProto(app_modern_with_deprecated_field));

  ASSERT_TRUE(migrated_app1);
  ASSERT_TRUE(migrated_app2);
  ASSERT_TRUE(migrated_app3);
  ASSERT_TRUE(migrated_app4);
  ASSERT_TRUE(migrated_app5);
  ASSERT_TRUE(migrated_app6);
  ASSERT_TRUE(migrated_app7);
  ASSERT_TRUE(migrated_app8);

  EXPECT_FALSE(
      migrated_app1->launch_handler().has_value());          // App 1: No change
  ASSERT_TRUE(migrated_app2->launch_handler().has_value());  // App 2: No change
  EXPECT_EQ(migrated_app2->launch_handler()->parsed_client_mode(),
            LaunchHandler::ClientMode::kNavigateNew);
  ASSERT_TRUE(migrated_app3->launch_handler().has_value());  // App 3: Migrated
  EXPECT_EQ(migrated_app3->launch_handler()->parsed_client_mode(),
            LaunchHandler::ClientMode::kNavigateNew);
  ASSERT_TRUE(migrated_app4->launch_handler().has_value());  // App 4: Migrated
  EXPECT_EQ(migrated_app4->launch_handler()->parsed_client_mode(),
            LaunchHandler::ClientMode::kNavigateExisting);
  ASSERT_TRUE(migrated_app5->launch_handler().has_value());  // App 5: Migrated
  EXPECT_EQ(migrated_app5->launch_handler()->parsed_client_mode(),
            LaunchHandler::ClientMode::kFocusExisting);
  ASSERT_TRUE(migrated_app6->launch_handler().has_value());  // App 6: Migrated
  EXPECT_EQ(
      migrated_app6->launch_handler()->parsed_client_mode(),
      LaunchHandler::ClientMode::kNavigateNew);  // Became unspecified -> auto
  ASSERT_TRUE(migrated_app7->launch_handler().has_value());  // App 7: Migrated
  EXPECT_EQ(migrated_app7->launch_handler()->parsed_client_mode(),
            LaunchHandler::ClientMode::kNavigateNew);
  ASSERT_TRUE(migrated_app8->launch_handler().has_value());  // App 8: Migrated
  EXPECT_EQ(migrated_app8->launch_handler()->parsed_client_mode(),
            LaunchHandler::ClientMode::kNavigateNew);

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest, MigrateScopeToRemoveRefAndQuery) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  // App 1: Clean scope
  proto::WebApp app_clean =
      CreateWebAppProtoForTesting("App 1", GURL("https://app1.com/start"));
  app_clean.set_scope("https://app1.com/");

  // App 2: Scope with query
  proto::WebApp app_query =
      CreateWebAppProtoForTesting("App 2", GURL("https://app2.com/start"));
  app_query.set_scope("https://app2.com/?query=1");

  // App 3: Scope with ref
  proto::WebApp app_ref =
      CreateWebAppProtoForTesting("App 3", GURL("https://app3.com/start"));
  app_ref.set_scope("https://app3.com/#ref");

  // App 4: Scope with query and ref
  proto::WebApp app_both =
      CreateWebAppProtoForTesting("App 4", GURL("https://app4.com/start"));
  app_both.set_scope("https://app4.com/path/?query=1#ref");

  // App 5: Invalid scope (should be skipped)
  proto::WebApp app_invalid =
      CreateWebAppProtoForTesting("App 5", GURL("https://app5.com/start"));
  app_invalid.set_scope("invalid-url");

  // App 6: No scope field (should be skipped)
  proto::WebApp app_no_scope =
      CreateWebAppProtoForTesting("App 6", GURL("https://app6.com/start"));
  app_no_scope.clear_scope();

  database_factory->WriteProtos(
      {app_clean, app_query, app_ref, app_both, app_invalid, app_no_scope});

  // Set version to 2 to trigger the migration to version 3.
  proto::DatabaseMetadata metadata;
  metadata.set_version(2);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;
  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  // Apps 2, 3, 4 should have been migrated.
  EXPECT_THAT(
      histograms.GetAllSamples("WebApp.Migrations.ScopeRefQueryRemoved"),
      base::BucketsAre(base::Bucket(/*min=*/3, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* migrated_app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_clean));
  const WebApp* migrated_app2 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_query));
  const WebApp* migrated_app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_ref));
  const WebApp* migrated_app4 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_both));
  const WebApp* migrated_app5 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_invalid));
  const WebApp* migrated_app6 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_no_scope));

  ASSERT_TRUE(migrated_app1);
  ASSERT_TRUE(migrated_app2);
  ASSERT_TRUE(migrated_app3);
  ASSERT_TRUE(migrated_app4);
  EXPECT_FALSE(migrated_app5);
  EXPECT_FALSE(migrated_app6);

  EXPECT_EQ(migrated_app1->scope(), GURL("https://app1.com/"));  // No change
  EXPECT_EQ(migrated_app2->scope(),
            GURL("https://app2.com/"));  // Query removed
  EXPECT_EQ(migrated_app3->scope(), GURL("https://app3.com/"));  // Ref removed
  EXPECT_EQ(migrated_app4->scope(),
            GURL("https://app4.com/path/"));  // Both removed

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

TEST_F(WebAppDatabaseMigrationTest, MigrateToRelativeManifestIdNoFragment) {
  FakeWebAppDatabaseFactory* database_factory =
      fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  ASSERT_TRUE(database_factory);

  GURL start_url1("https://app1.com/start");
  webapps::ManifestId manifest_id1 =
      GenerateManifestIdFromStartUrlOnly(start_url1);
  std::string correct_relative_path1 = RelativeManifestIdPath(manifest_id1);

  // App 1: Correct relative_manifest_id
  proto::WebApp app_correct = CreateWebAppProtoForTesting("App 1", start_url1);
  app_correct.mutable_sync_data()->set_relative_manifest_id(
      correct_relative_path1);

  // App 2: Missing relative_manifest_id
  GURL start_url2("https://app2.com/start?p=1");
  webapps::ManifestId manifest_id2 =
      GenerateManifestIdFromStartUrlOnly(start_url2);
  std::string correct_relative_path2 = RelativeManifestIdPath(manifest_id2);
  proto::WebApp app_missing = CreateWebAppProtoForTesting("App 2", start_url2);
  app_missing.mutable_sync_data()->clear_relative_manifest_id();
  webapps::AppId app_id2 = GenerateAppIdFromManifestId(manifest_id2);

  // App 3: relative_manifest_id with fragment
  GURL start_url3("https://app3.com/start#frag");
  webapps::ManifestId manifest_id3 =
      GenerateManifestIdFromStartUrlOnly(start_url3);
  std::string correct_relative_path3 = RelativeManifestIdPath(manifest_id3);
  proto::WebApp app_fragment = CreateWebAppProtoForTesting("App 3", start_url3);
  // Manually set incorrect path with fragment
  app_fragment.mutable_sync_data()->set_relative_manifest_id(
      correct_relative_path3 + "#fragment");

  database_factory->WriteProtos({app_correct, app_missing, app_fragment});

  // Set version to 2 to trigger the migration to version 3.
  proto::DatabaseMetadata metadata;
  metadata.set_version(2);
  database_factory->WriteMetadata(metadata);

  base::HistogramTester histograms;
  // Start the system, which should run the migration.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  // App 3 had fragment removed.
  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.RelativeManifestIdFragmentRemoved"),
              base::BucketsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  // Apps 2 and 3 were populated or fixed.
  EXPECT_THAT(histograms.GetAllSamples(
                  "WebApp.Migrations.RelativeManifestIdPopulatedOrFixed"),
              base::BucketsAre(base::Bucket(/*min=*/2, /*count=*/1)));

  WebAppRegistrar& registrar = fake_provider().registrar_unsafe();

  // Verify migration results
  const WebApp* migrated_app1 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_correct));
  const WebApp* migrated_app2 = registrar.GetAppById(app_id2);
  const WebApp* migrated_app3 =
      registrar.GetAppById(GetAppIdFromWebAppProto(app_fragment));

  ASSERT_TRUE(migrated_app1);
  ASSERT_TRUE(migrated_app2) << app_id2;
  ASSERT_TRUE(migrated_app3);

  // Check the relative_manifest_id in the sync_proto
  EXPECT_TRUE(migrated_app1->sync_proto().has_relative_manifest_id());
  EXPECT_EQ(migrated_app1->sync_proto().relative_manifest_id(),
            correct_relative_path1);  // No change

  EXPECT_TRUE(migrated_app2->sync_proto().has_relative_manifest_id());
  EXPECT_EQ(migrated_app2->sync_proto().relative_manifest_id(),
            correct_relative_path2);  // Populated

  EXPECT_TRUE(migrated_app3->sync_proto().has_relative_manifest_id());
  EXPECT_EQ(migrated_app3->sync_proto().relative_manifest_id(),
            correct_relative_path3);  // Fragment removed

  // Check that the data was also updated in the database.
  VerifyDatabaseRegistryEqualToRegistrar(&fake_provider());
}

}  // namespace
}  // namespace web_app
