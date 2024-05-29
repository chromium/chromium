// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

class ClearBrowsingDataCommandTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();
    web_app_provider_ = FakeWebAppProvider::Get(profile());
    web_app_provider_->StartWithSubsystems();
  }

  void Init() {
    base::RunLoop run_loop;
    provider()->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  FakeWebAppProvider* provider() { return web_app_provider_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> web_app_provider_ = nullptr;
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
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(launch_time1,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(launch_time2,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id3));

  base::test::TestFuture<void> future;
  provider()->scheduler().ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id3));
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
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(launch_time1,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(launch_time2,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id3));

  base::test::TestFuture<void> future;
  provider()->scheduler().ClearWebAppBrowsingData(
      base::Time() + base::Seconds(5), base::Time() + base::Seconds(15),
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(launch_time1,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            provider()->registrar_unsafe().GetAppLastLaunchTime(app_id3));
}

TEST_F(ClearBrowsingDataCommandTest,
       ClearLastLaunchTimeCalledBeforeWebAppProviderIsReady) {
  base::test::TestFuture<void> future;
  provider()->scheduler().ClearWebAppBrowsingData(
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
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(badging_time1,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(badging_time2,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id3));

  base::test::TestFuture<void> future;
  provider()->scheduler().ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id3));
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
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(badging_time1,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(badging_time2,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id3));

  base::test::TestFuture<void> future;
  provider()->scheduler().ClearWebAppBrowsingData(
      base::Time() + base::Seconds(5), base::Time() + base::Seconds(15),
      future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(badging_time1,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            provider()->registrar_unsafe().GetAppLastBadgingTime(app_id3));
}

}  // namespace web_app
