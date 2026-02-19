// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

class ClearBrowsingDataCommandTest : public WebAppTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
    WebAppTest::SetUp();

    auto association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    association_manager->set_pass_through(true);
    fake_provider().SetOriginAssociationManager(std::move(association_manager));

    fake_provider().StartWithSubsystems();
  }

  void Init() {
    base::RunLoop run_loop;
    fake_provider().on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ClearBrowsingDataCommandTest, ClearLastLaunchTimeForAllTimes) {
  Init();

  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  auto launch_time1 = base::Time();
  auto app_id1 = web_app1->app_id();

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  auto launch_time2 = base::Time() + base::Seconds(10);
  web_app2->SetLastLaunchTime(launch_time2);
  auto app_id2 = web_app2->app_id();

  auto web_app3 = test::CreateWebApp(GURL("https://example.com/path3"));
  auto launch_time3 = base::Time() + base::Seconds(20);
  web_app3->SetLastLaunchTime(launch_time3);
  auto app_id3 = web_app3->app_id();

  {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, fake_provider().registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(launch_time1,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(launch_time2,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id3));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id3));
}

TEST_F(ClearBrowsingDataCommandTest, ClearLastLaunchTimeForSpecificTimeRange) {
  Init();

  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  auto launch_time1 = base::Time();
  auto app_id1 = web_app1->app_id();

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  auto launch_time2 = base::Time() + base::Seconds(10);
  web_app2->SetLastLaunchTime(launch_time2);
  auto app_id2 = web_app2->app_id();

  auto web_app3 = test::CreateWebApp(GURL("https://example.com/path3"));
  auto launch_time3 = base::Time() + base::Seconds(20);
  web_app3->SetLastLaunchTime(launch_time3);
  auto app_id3 = web_app3->app_id();

  {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, fake_provider().registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(launch_time1,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(launch_time2,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id3));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().ClearWebAppBrowsingData(
      base::Time() + base::Seconds(5), base::Time() + base::Seconds(15),
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(launch_time1,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            fake_provider().registrar_unsafe().GetAppLastLaunchTime(app_id3));
}

TEST_F(ClearBrowsingDataCommandTest,
       ClearLastLaunchTimeCalledBeforeWebAppProviderIsReady) {
  base::test::TestFuture<void> future;
  fake_provider().scheduler().ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), future.GetCallback());
  Init();
  EXPECT_TRUE(future.Wait());
}

TEST_F(ClearBrowsingDataCommandTest, ClearLastBadgingTimeForAllTimes) {
  Init();

  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  auto badging_time1 = base::Time();
  auto app_id1 = web_app1->app_id();

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  auto badging_time2 = base::Time() + base::Seconds(10);
  web_app2->SetLastBadgingTime(badging_time2);
  auto app_id2 = web_app2->app_id();

  auto web_app3 = test::CreateWebApp(GURL("https://example.com/path3"));
  auto badging_time3 = base::Time() + base::Seconds(20);
  web_app3->SetLastBadgingTime(badging_time3);
  auto app_id3 = web_app3->app_id();

  {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, fake_provider().registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(badging_time1,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(badging_time2,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id3));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id3));
}

TEST_F(ClearBrowsingDataCommandTest, ClearLastBadgingTimeForSpecificTimeRange) {
  Init();

  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  auto badging_time1 = base::Time();
  auto app_id1 = web_app1->app_id();

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  auto badging_time2 = base::Time() + base::Seconds(10);
  web_app2->SetLastBadgingTime(badging_time2);
  auto app_id2 = web_app2->app_id();

  auto web_app3 = test::CreateWebApp(GURL("https://example.com/path3"));
  auto badging_time3 = base::Time() + base::Seconds(20);
  web_app3->SetLastBadgingTime(badging_time3);
  auto app_id3 = web_app3->app_id();

  {
    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, fake_provider().registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(badging_time1,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(badging_time2,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id3));

  base::test::TestFuture<void> future;
  fake_provider().scheduler().ClearWebAppBrowsingData(
      base::Time() + base::Seconds(5), base::Time() + base::Seconds(15),
      future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(badging_time1,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(base::Time(),
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            fake_provider().registrar_unsafe().GetAppLastBadgingTime(app_id3));
}

TEST_F(ClearBrowsingDataCommandTest, ClearMigrationApps) {
  Init();

  GURL source_url("https://source.com");
  auto source_app_id =
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

  base::test::TestFuture<WebAppInstallFromMigrateFromFieldResult>
      install_future;
  fake_provider().scheduler().ScheduleWebAppInstallFromMigrateFromField(
      web_contents()->GetWeakPtr(), std::move(manifest),
      install_future.GetCallback());

  EXPECT_EQ(install_future.Get(),
            WebAppInstallFromMigrateFromFieldResult::kSuccessInstalled);
  const WebApp* target_app =
      fake_provider().registrar_unsafe().GetAppById(target_app_id);
  ASSERT_TRUE(target_app);
  EXPECT_EQ(target_app->install_state(),
            proto::InstallState::SUGGESTED_FROM_MIGRATION);

  base::test::TestFuture<void> clear_future;
  fake_provider().scheduler().ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());

  // Wait for the scheduled uninstalls from ClearWebAppBrowsingData.
  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(fake_provider().registrar_unsafe().GetAppById(target_app_id));

  EXPECT_TRUE(fake_provider().registrar_unsafe().GetAppById(source_app_id));
}

}  // namespace web_app
