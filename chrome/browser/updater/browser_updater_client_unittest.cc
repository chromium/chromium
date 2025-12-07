// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/browser/updater/browser_updater_client_testutils.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BrowserUpdaterClientTest, Reuse) {
  scoped_refptr<BrowserUpdaterClient> user1 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess, {}),
      updater::UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> user2 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess, {}),
      updater::UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> system1 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess, {}),
      updater::UpdaterScope::kSystem);
  scoped_refptr<BrowserUpdaterClient> system2 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess, {}),
      updater::UpdaterScope::kSystem);
  EXPECT_EQ(user1, user2);
  EXPECT_EQ(system1, system2);
  EXPECT_NE(system1, user1);
  EXPECT_NE(system1, user2);
  EXPECT_NE(system2, user1);
  EXPECT_NE(system2, user2);
}

TEST(BrowserUpdaterClientTest, CallbackNumber) {
  base::test::SingleThreadTaskEnvironment task_environment;

  {
    int num_called = 0;
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(updater::UpdateService::Result::kSuccess, {}),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              num_called++;
              loop.QuitWhenIdle();
            }));
    loop.Run();
    EXPECT_EQ(num_called, 2);
  }

  {
    int num_called = 0;
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(
            updater::UpdateService::Result::kUpdateCheckFailed, {}),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              num_called++;
              loop.QuitWhenIdle();
            }));
    loop.Run();
    EXPECT_EQ(num_called, 2);
  }

  {
    int num_called = 0;
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(
            updater::UpdateService::Result::kIPCConnectionFailed, {}),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              num_called++;
              loop.QuitWhenIdle();
            }));
    loop.Run();
    EXPECT_EQ(num_called, 3);
  }
}

TEST(BrowserUpdaterClientTest, StoreRetrieveLastUpdateState) {
  base::test::SingleThreadTaskEnvironment task_environment;
  {
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(updater::UpdateService::Result::kSuccess, {}),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              loop.QuitWhenIdle();
            }));
    loop.Run();
  }
  EXPECT_TRUE(BrowserUpdaterClient::GetLastOnDemandUpdateState());
  EXPECT_EQ(BrowserUpdaterClient::GetLastOnDemandUpdateState()->state,
            updater::UpdateService::UpdateState::State::kNoUpdate);
}

TEST(BrowserUpdaterClientTest, StoreRetrieveLastAppState) {
  base::test::SingleThreadTaskEnvironment task_environment;
  updater::UpdateService::AppState app1;
  app1.app_id = updater::kUpdaterAppId;
  updater::UpdateService::AppState app2;
  app2.app_id = BrowserUpdaterClient::GetAppId();
  app2.ecp = BrowserUpdaterClient::GetExpectedEcp();
  {
    base::RunLoop loop;
    bool is_registered = false;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(updater::UpdateService::Result::kSuccess,
                                 {
                                     app1,
                                     app2,
                                 }),
        updater::UpdaterScope::kUser)
        ->IsBrowserRegistered(base::BindLambdaForTesting([&](bool registered) {
          is_registered = registered;
          loop.QuitWhenIdle();
        }));
    loop.Run();
    EXPECT_TRUE(is_registered);
  }
  EXPECT_TRUE(BrowserUpdaterClient::GetLastKnownBrowserRegistration());
  EXPECT_TRUE(BrowserUpdaterClient::GetLastKnownUpdaterRegistration());
}
