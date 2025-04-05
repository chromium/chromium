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

  EXPECT_THAT(histograms.GetAllSamples("WebApp.Migrations.ShortcutAppsToDiy"),
              base::BucketsAre(base::Bucket(2, 1)));

  auto* provider = WebAppProvider::GetForWebApps(profile());
  WebAppRegistrar& registrar = provider->registrar_unsafe();

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
}

}  // namespace
}  // namespace web_app
