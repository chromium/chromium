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
#include "base/version.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

class FakeUpdateService : public UpdateService {
 public:
  FakeUpdateService() = default;

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
    std::move(callback).Run(Result::kSuccess);
  }
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              base::RepeatingCallback<void(const UpdateState&)> state_update,
              base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(Result::kSuccess);
  }
  void UpdateAll(base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(Result::kSuccess);
  }
  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               Priority priority,
               base::RepeatingCallback<void(const UpdateState&)> state_update,
               base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(Result::kSuccess);
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
    std::move(callback).Run(Result::kSuccess);
  }

 private:
  ~FakeUpdateService() override = default;
};

base::RepeatingCallback<scoped_refptr<UpdateService>()> MakeFakeService() {
  return base::BindRepeating([] {
    return static_cast<scoped_refptr<UpdateService>>(
        base::MakeRefCounted<FakeUpdateService>());
  });
}

}  // namespace

}  // namespace updater

TEST(BrowserUpdaterClientTest, Reuse) {
  scoped_refptr<BrowserUpdaterClient> user1 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(), updater::UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> user2 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(), updater::UpdaterScope::kUser);
  scoped_refptr<BrowserUpdaterClient> system1 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(), updater::UpdaterScope::kSystem);
  scoped_refptr<BrowserUpdaterClient> system2 = BrowserUpdaterClient::Create(
      updater::MakeFakeService(), updater::UpdaterScope::kSystem);
  EXPECT_EQ(user1, user2);
  EXPECT_EQ(system1, system2);
  EXPECT_NE(system1, user1);
  EXPECT_NE(system1, user2);
  EXPECT_NE(system2, user1);
  EXPECT_NE(system2, user2);
}
