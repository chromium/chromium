// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/resolve_web_app_pending_migration_info_command.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/model/pending_migration_info.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

constexpr char kSourceAppUrl[] = "https://source.example.com/";
constexpr char kTargetAppUrl[] = "https://target.example.com/";

}  // namespace

class ResolveWebAppPendingMigrationInfoCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  void RunCommand() {
    base::test::TestFuture<void> future;
    provider()->scheduler().ScheduleResolveWebAppPendingMigrationInfo(
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }
};

TEST_F(ResolveWebAppPendingMigrationInfoCommandTest, NoApps) {
  RunCommand();
  EXPECT_EQ(0u, provider()->registrar_unsafe().GetAppIds().size());
}

TEST_F(ResolveWebAppPendingMigrationInfoCommandTest, SingleMigration) {
  auto app_id_source =
      test::InstallDummyWebApp(profile(), "Source App", GURL(kSourceAppUrl));
  auto app_id_target =
      test::InstallDummyWebApp(profile(), "Target App", GURL(kTargetAppUrl));

  // Set up migration source on target app.
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    WebApp* app_target = update->UpdateApp(app_id_target);
    std::vector<proto::WebAppMigrationSource> sources;
    proto::WebAppMigrationSource source;
    source.set_manifest_id(
        GenerateManifestIdFromStartUrlOnly(GURL(kSourceAppUrl)).spec());
    source.set_behavior(proto::WEB_APP_MIGRATION_BEHAVIOR_FORCE);
    sources.push_back(source);
    app_target->SetValidatedMigrationSources(sources);
  }

  RunCommand();

  const WebApp* app_source =
      provider()->registrar_unsafe().GetAppById(app_id_source);
  ASSERT_TRUE(app_source->pending_migration_info().has_value());
  EXPECT_EQ(GenerateManifestIdFromStartUrlOnly(GURL(kTargetAppUrl)).spec(),
            app_source->pending_migration_info()->manifest_id());
  EXPECT_EQ(MigrationBehavior::kForce,
            app_source->pending_migration_info()->behavior());
}

TEST_F(ResolveWebAppPendingMigrationInfoCommandTest, CleanupOldMigration) {
  auto app_id_source =
      test::InstallDummyWebApp(profile(), "Source App", GURL(kSourceAppUrl));

  // Set existing pending migration info that is no longer valid.
  {
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    WebApp* app_source = update->UpdateApp(app_id_source);
    PendingMigrationInfo info(webapps::ManifestId(GURL(kTargetAppUrl)),
                              MigrationBehavior::kSuggest);
    app_source->SetPendingMigrationInfo(info);
  }

  RunCommand();

  const WebApp* app_source =
      provider()->registrar_unsafe().GetAppById(app_id_source);
  EXPECT_FALSE(app_source->pending_migration_info().has_value());
}

}  // namespace web_app
