// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_COMMAND_UNITTEST_CC_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_COMMAND_UNITTEST_CC_

#include "chrome/browser/web_applications/commands/web_app_install_from_migrate_from_field_command.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {
namespace {

class WebAppInstallFromMigrateFromFieldCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
    WebAppTest::SetUp();

    auto* provider = FakeWebAppProvider::Get(profile());
    auto association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    association_manager->set_pass_through(true);
    provider->SetOriginAssociationManager(std::move(association_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }
  WebAppCommandManager& command_manager() {
    return provider().command_manager();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppInstallFromMigrateFromFieldCommandTest, NoSourceAppInstalled) {
  GURL target_url("https://target.com");
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = target_url;
  manifest->start_url = target_url;
  manifest->scope = target_url;

  auto migrate_from = blink::mojom::ManifestMigrateFrom::New();
  migrate_from->id = GURL("https://source.com");
  manifest->migrate_from.push_back(std::move(migrate_from));

  webapps::AppId target_app_id = GenerateAppIdFromManifest(*manifest);

  base::test::TestFuture<WebAppInstallFromMigrateFromFieldResult> future;
  provider().scheduler().ScheduleWebAppInstallFromMigrateFromField(
      web_contents()->GetWeakPtr(), std::move(manifest), future.GetCallback());

  EXPECT_EQ(future.Get(),
            WebAppInstallFromMigrateFromFieldResult::kNoSourceAppInstalled);
  EXPECT_FALSE(registrar().GetAppById(target_app_id));
}

TEST_F(WebAppInstallFromMigrateFromFieldCommandTest, SuccessInstalled) {
  GURL source_url("https://source.com");
  test::InstallDummyWebApp(profile(), "Source App", source_url);

  GURL target_url("https://target.com");
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = target_url;
  manifest->start_url = target_url;
  manifest->scope = target_url;
  manifest->name = u"Target App";

  auto migrate_from = blink::mojom::ManifestMigrateFrom::New();
  migrate_from->id = source_url;
  manifest->migrate_from.push_back(std::move(migrate_from));

  webapps::AppId target_app_id = GenerateAppIdFromManifest(*manifest);

  base::test::TestFuture<WebAppInstallFromMigrateFromFieldResult> future;
  provider().scheduler().ScheduleWebAppInstallFromMigrateFromField(
      web_contents()->GetWeakPtr(), std::move(manifest), future.GetCallback());

  EXPECT_EQ(future.Get(),
            WebAppInstallFromMigrateFromFieldResult::kSuccessInstalled);
  const WebApp* target_app = registrar().GetAppById(target_app_id);
  ASSERT_TRUE(target_app);
  EXPECT_EQ(target_app->install_state(),
            proto::InstallState::SUGGESTED_FROM_MIGRATION);
}

// Note: More extensive testing of the installation logic (including updates)
// is covered by MigrationTargetInstallJobTest in
// c/b/web_applications/jobs/migration_target_install_job_unittest.cc.

}  // namespace
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_COMMAND_UNITTEST_CC_
