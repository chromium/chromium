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
#include "chrome/browser/updater/updater.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(BrowserUpdaterClientTest, Reuse) {
  scoped_refptr<BrowserUpdaterClient> user1 = BrowserUpdaterClient::Create(
      MakeFakeService(UpdateService::Result::kSuccess, {}),
      UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> user2 = BrowserUpdaterClient::Create(
      MakeFakeService(UpdateService::Result::kSuccess, {}),
      UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> system1 = BrowserUpdaterClient::Create(
      MakeFakeService(UpdateService::Result::kSuccess, {}),
      UpdaterScope::kSystem);
  scoped_refptr<BrowserUpdaterClient> system2 = BrowserUpdaterClient::Create(
      MakeFakeService(UpdateService::Result::kSuccess, {}),
      UpdaterScope::kSystem);
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
        MakeFakeService(UpdateService::Result::kSuccess, {}),
        UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const UpdateService::UpdateState& status) {
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
        MakeFakeService(UpdateService::Result::kUpdateCheckFailed, {}),
        UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const UpdateService::UpdateState& status) {
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
        MakeFakeService(UpdateService::Result::kIPCConnectionFailed, {}),
        UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const UpdateService::UpdateState& status) {
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
        MakeFakeService(UpdateService::Result::kSuccess, {}),
        UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const UpdateService::UpdateState& status) {
              loop.QuitWhenIdle();
            }));
    loop.Run();
  }
  EXPECT_TRUE(GetLastOnDemandUpdateState());
  EXPECT_EQ(GetLastOnDemandUpdateState()->state,
            UpdateService::UpdateState::State::kNoUpdate);
}

TEST(BrowserUpdaterClientTest, StoreRetrieveLastAppState) {
  base::test::SingleThreadTaskEnvironment task_environment;
  UpdateService::AppState app1;
  app1.app_id = kUpdaterAppId;
  UpdateService::AppState app2;
  app2.app_id = BrowserUpdaterClient::GetAppId();
  app2.ecp = BrowserUpdaterClient::GetExpectedEcp();
  {
    base::RunLoop loop;
    bool is_registered = false;
    BrowserUpdaterClient::Create(
        MakeFakeService(UpdateService::Result::kSuccess,
                        {
                            app1,
                            app2,
                        }),
        UpdaterScope::kUser)
        ->IsBrowserRegistered(base::BindLambdaForTesting([&](bool registered) {
          is_registered = registered;
          loop.QuitWhenIdle();
        }));
    loop.Run();
    EXPECT_TRUE(is_registered);
  }
  EXPECT_TRUE(GetLastKnownBrowserRegistration());
  EXPECT_TRUE(GetLastKnownUpdaterRegistration());
}

}  // namespace updater
