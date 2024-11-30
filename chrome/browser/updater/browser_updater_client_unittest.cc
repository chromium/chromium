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
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

class FakeUpdateService : public UpdateService {
 public:
  explicit FakeUpdateService(UpdateService::Result result) : result_(result) {}

  // Overrides for UpdateService
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override {
    std::move(callback).Run(base::Version("1.2.3.4"));
  }
  void FetchPolicies(base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(0);
  }
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(0);
  }
  void GetAppStates(base::OnceCallback<void(const std::vector<AppState>&)>
                        callback) override {
    std::move(callback).Run({});
  }
  void RunPeriodicTasks(base::OnceClosure callback) override {
    std::move(callback).Run();
  }
  void CheckForUpdate(
      const std::string& app_id,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              base::RepeatingCallback<void(const UpdateState&)> state_update,
              base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }
  void UpdateAll(base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }
  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               Priority priority,
               base::RepeatingCallback<void(const UpdateState&)> state_update,
               base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }
  void CancelInstalls(const std::string& app_id) override {}
  void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }

 private:
  Result result_;

  ~FakeUpdateService() override = default;
};

base::RepeatingCallback<scoped_refptr<UpdateService>()> MakeFakeService(
    UpdateService::Result result) {
  return base::BindRepeating(
      [](UpdateService::Result result) {
        return static_cast<scoped_refptr<UpdateService>>(
            base::MakeRefCounted<FakeUpdateService>(result));
      },
      result);
}

}  // namespace

}  // namespace updater

TEST(BrowserUpdaterClientTest, Reuse) {
  scoped_refptr<BrowserUpdaterClient> user1 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess),
      updater::UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> user2 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess),
      updater::UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> system1 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess),
      updater::UpdaterScope::kSystem);
  scoped_refptr<BrowserUpdaterClient> system2 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(updater::UpdateService::Result::kSuccess),
      updater::UpdaterScope::kSystem);
  EXPECT_EQ(user1, user2);
  EXPECT_EQ(system1, system2);
  EXPECT_NE(system1, user1);
  EXPECT_NE(system1, user2);
  EXPECT_NE(system2, user1);
  EXPECT_NE(system2, user2);
}

TEST(BrowserUpdaterClient, CallbackNumber) {
  base::test::SingleThreadTaskEnvironment task_environment;

  {
    int num_called = 0;
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(updater::UpdateService::Result::kSuccess),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              num_called++;
              loop.QuitWhenIdle();
            }));
    loop.Run();
    EXPECT_EQ(num_called, 1);
  }

  {
    int num_called = 0;
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(
            updater::UpdateService::Result::kUpdateCheckFailed),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              num_called++;
              loop.QuitWhenIdle();
            }));
    loop.Run();
    EXPECT_EQ(num_called, 1);
  }

  {
    int num_called = 0;
    base::RunLoop loop;
    BrowserUpdaterClient::Create(
        updater::MakeFakeService(
            updater::UpdateService::Result::kIPCConnectionFailed),
        updater::UpdaterScope::kUser)
        ->CheckForUpdate(base::BindLambdaForTesting(
            [&](const updater::UpdateService::UpdateState& status) {
              num_called++;
              loop.QuitWhenIdle();
            }));
    loop.Run();
    EXPECT_EQ(num_called, 2);
  }
}
