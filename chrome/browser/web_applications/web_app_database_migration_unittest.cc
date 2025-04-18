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

  Registry db_registry = database_factory->ReadRegistry();
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

}  // namespace
}  // namespace web_app
