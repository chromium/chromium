// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"

namespace web_app {

class ClearBrowsingDataCommandTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();
    web_app_provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app_provider_->StartWithSubsystems();
  }

  void Init() {
    base::RunLoop run_loop;
    web_app_provider_->on_registry_ready().Post(FROM_HERE,
                                                run_loop.QuitClosure());
    run_loop.Run();
  }

  FakeWebAppProvider* provider() { return web_app_provider_; }

 private:
  raw_ptr<FakeWebAppProvider> web_app_provider_;
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
    web_app::ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar().GetAppIds().size());
  EXPECT_EQ(launch_time1,
            provider()->registrar().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(launch_time2,
            provider()->registrar().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            provider()->registrar().GetAppLastLaunchTime(app_id3));

  bool callback_invoked = false;
  web_app::ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), provider(),
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastLaunchTime(app_id3));
  EXPECT_TRUE(callback_invoked);
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
    web_app::ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar().GetAppIds().size());
  EXPECT_EQ(launch_time1,
            provider()->registrar().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(launch_time2,
            provider()->registrar().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            provider()->registrar().GetAppLastLaunchTime(app_id3));

  bool callback_invoked = false;
  web_app::ClearWebAppBrowsingData(
      base::Time() + base::Seconds(5), base::Time() + base::Seconds(15),
      provider(),
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  EXPECT_EQ(launch_time1,
            provider()->registrar().GetAppLastLaunchTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastLaunchTime(app_id2));
  EXPECT_EQ(launch_time3,
            provider()->registrar().GetAppLastLaunchTime(app_id3));
  EXPECT_TRUE(callback_invoked);
}

TEST_F(ClearBrowsingDataCommandTest,
       ClearLastLaunchTimeCalledBeforeWebAppProviderIsReady) {
  bool callback_invoked = false;
  web_app::ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), provider(),
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  EXPECT_FALSE(callback_invoked);

  Init();

  EXPECT_TRUE(callback_invoked);
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
    web_app::ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar().GetAppIds().size());
  EXPECT_EQ(badging_time1,
            provider()->registrar().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(badging_time2,
            provider()->registrar().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            provider()->registrar().GetAppLastBadgingTime(app_id3));

  bool callback_invoked = false;
  web_app::ClearWebAppBrowsingData(
      base::Time(), base::Time::Now(), provider(),
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastBadgingTime(app_id3));
  EXPECT_TRUE(callback_invoked);
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
    web_app::ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app1));
    update->CreateApp(std::move(web_app2));
    update->CreateApp(std::move(web_app3));
  }

  EXPECT_EQ(3UL, provider()->registrar().GetAppIds().size());
  EXPECT_EQ(badging_time1,
            provider()->registrar().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(badging_time2,
            provider()->registrar().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            provider()->registrar().GetAppLastBadgingTime(app_id3));

  bool callback_invoked = false;
  web_app::ClearWebAppBrowsingData(
      base::Time() + base::Seconds(5), base::Time() + base::Seconds(15),
      provider(),
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  EXPECT_EQ(badging_time1,
            provider()->registrar().GetAppLastBadgingTime(app_id1));
  EXPECT_EQ(base::Time(),
            provider()->registrar().GetAppLastBadgingTime(app_id2));
  EXPECT_EQ(badging_time3,
            provider()->registrar().GetAppLastBadgingTime(app_id3));
  EXPECT_TRUE(callback_invoked);
}

}  // namespace web_app
