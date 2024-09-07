// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl_inactive.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdateServiceImplInactiveTest, All) {
  base::test::TaskEnvironment task_environment;
  scoped_refptr<UpdateService> update_service = MakeInactiveUpdateService();
  {
    base::RunLoop run_loop;
    update_service->GetVersion(
        base::BindLambdaForTesting([&run_loop](const base::Version& version) {
          EXPECT_FALSE(version.IsValid());
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->FetchPolicies(
        base::BindLambdaForTesting([&run_loop](int result) {
          EXPECT_EQ(result, -1);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->RegisterApp(
        RegistrationRequest(),
        base::BindLambdaForTesting([&run_loop](int result) {
          EXPECT_EQ(result, -1);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->GetAppStates(base::BindLambdaForTesting(
        [&run_loop](const std::vector<UpdateService::AppState>& app_states) {
          EXPECT_TRUE(app_states.empty());
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->RunPeriodicTasks(
        base::BindLambdaForTesting([&run_loop] { run_loop.Quit(); }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->CheckForUpdate(
        /*app_id=*/"", UpdateService::Priority::kForeground,
        UpdateService::PolicySameVersionUpdate::kNotAllowed,
        base::RepeatingCallback<void(const UpdateService::UpdateState&)>(),
        base::BindLambdaForTesting([&run_loop](UpdateService::Result result) {
          EXPECT_EQ(result, UpdateService::Result::kInactive);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->Update(
        /*app_id=*/"",
        /*install_data_index=*/"", UpdateService::Priority::kForeground,
        UpdateService::PolicySameVersionUpdate::kNotAllowed,
        base::RepeatingCallback<void(const UpdateService::UpdateState&)>(),
        base::BindLambdaForTesting([&run_loop](UpdateService::Result result) {
          EXPECT_EQ(result, UpdateService::Result::kInactive);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->UpdateAll(
        base::RepeatingCallback<void(const UpdateService::UpdateState&)>(),
        base::BindLambdaForTesting([&run_loop](UpdateService::Result result) {
          EXPECT_EQ(result, UpdateService::Result::kInactive);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    update_service->Install(
        RegistrationRequest(),
        /*client_install_data=*/"",
        /*install_data_index=*/"", UpdateService::Priority::kForeground,
        base::RepeatingCallback<void(const UpdateService::UpdateState&)>(),
        base::BindLambdaForTesting([&run_loop](UpdateService::Result result) {
          EXPECT_EQ(result, UpdateService::Result::kInactive);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    // This function does not take a callback parameter.
    update_service->CancelInstalls(/*app_id=*/"");
  }
  {
    base::RunLoop run_loop;
    update_service->RunInstaller(
        /*app_id=*/"",
        /*installer_path=*/base::FilePath(),
        /*install_args=*/"",
        /*install_data=*/"",
        /*install_settings=*/"",
        base::RepeatingCallback<void(const UpdateService::UpdateState&)>(),
        base::BindLambdaForTesting([&run_loop](UpdateService::Result result) {
          EXPECT_EQ(result, UpdateService::Result::kInactive);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace updater
