// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_testutils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/version.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/updater/update_service.h"

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace updater {

namespace {

class FakeUpdateService : public UpdateService {
 public:
  FakeUpdateService(UpdateService::Result result,
                    const std::vector<UpdateService::AppState>& states)
      : result_(result), states_(states) {}

  // Overrides for UpdateService
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override {
    std::move(callback).Run(base::Version("1.2.3.4"));
  }
  void FetchPolicies(policy::PolicyFetchReason,
                     base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(0);
  }
  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(0);
  }
  void GetAppStates(base::OnceCallback<void(const std::vector<AppState>&)>
                        callback) override {
    std::move(callback).Run(states_);
  }
  void RunPeriodicTasks(base::OnceClosure callback) override {
    std::move(callback).Run();
  }
  void CheckForUpdate(
      const std::string& app_id,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }
  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              const std::string& language,
              base::RepeatingCallback<void(const UpdateState&)> state_update,
              base::OnceCallback<void(Result)> callback) override {
    UpdateState state;
    state.state = result_ == UpdateService::Result::kSuccess
                      ? UpdateState::State::kNoUpdate
                      : UpdateState::State::kUpdateError;
    state_update.Run(state);
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
               const std::string& language,
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
      const std::string& language,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) override {
    std::move(callback).Run(result_);
  }
  void GetUpdaterState(
      base::OnceCallback<void(const UpdaterState&)> callback) override {
    std::move(callback).Run(UpdaterState());
  }
  void GetPoliciesJson(
      base::OnceCallback<void(const std::string&)> callback) override {
    std::move(callback).Run("");
  }

 private:
  ~FakeUpdateService() override = default;

  const Result result_;
  const std::vector<UpdateService::AppState> states_;
};

}  // namespace

base::RepeatingCallback<scoped_refptr<UpdateService>()> MakeFakeService(
    UpdateService::Result result,
    const std::vector<UpdateService::AppState>& states) {
  return base::BindRepeating(
      [](UpdateService::Result result,
         const std::vector<UpdateService::AppState>& states) {
        return static_cast<scoped_refptr<UpdateService>>(
            base::MakeRefCounted<FakeUpdateService>(result, states));
      },
      result, states);
}

base::ScopedClosureRunner OverrideService(
    UpdateService::Result result,
    const std::vector<UpdateService::AppState>& states) {
  return base::ScopedClosureRunner(base::BindOnce(
      [](scoped_refptr<BrowserUpdaterClient>) {},
      BrowserUpdaterClient::Create(MakeFakeService(result, states),
                                   UpdaterScope::kUser)));
}

void LoadAppStates() {
  base::RunLoop loop;
  BrowserUpdaterClient::Create(updater::UpdaterScope::kUser)
      ->IsBrowserRegistered(
          base::BindOnce([](bool) {}).Then(loop.QuitClosure()));
  loop.Run();
}

std::string GetAppId() {
  return BrowserUpdaterClient::GetAppId();
}

base::FilePath GetExpectedEcp() {
  return BrowserUpdaterClient::GetExpectedEcp();
}

}  // namespace updater
