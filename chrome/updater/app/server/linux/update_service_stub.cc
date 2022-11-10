// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/linux/update_service_stub.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process_handle.h"
#include "base/version.h"
#include "chrome/updater/app/server/linux/mojom/updater_service.mojom-forward.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/registration_data.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace updater {
namespace {

// Helper functions for converting between mojom types and their native
// counterparts.
[[nodiscard]] updater::RegistrationRequest MakeRegistrationRequest(
    const mojom::RegistrationRequestPtr& mojom) {
  CHECK(mojom);

  updater::RegistrationRequest request;
  request.app_id = mojom->app_id;
  request.brand_code = mojom->brand_code;
  request.brand_path = mojom->brand_path;
  request.ap = mojom->ap;
  request.version = base::Version(mojom->version);
  request.existence_checker_path = mojom->existence_checker_path;
  return request;
}

[[nodiscard]] mojom::AppStatePtr MakeMojoAppState(
    const updater::UpdateService::AppState& app_state) {
  return mojom::AppState::New(app_state.app_id, app_state.version.GetString(),
                              app_state.ap, app_state.brand_code,
                              app_state.brand_path, app_state.ecp);
}

[[nodiscard]] mojom::UpdateStatePtr MakeMojoUpdateState(
    const updater::UpdateService::UpdateState& update_state) {
  return mojom::UpdateState::New(
      update_state.app_id,
      static_cast<mojom::UpdateState::State>(update_state.state),
      update_state.next_version.GetString(), update_state.downloaded_bytes,
      update_state.total_bytes, update_state.install_progress,
      static_cast<mojom::UpdateService::ErrorCategory>(
          update_state.error_category),
      update_state.error_code, update_state.extra_code1,
      update_state.installer_text, update_state.installer_cmd_line);
}

// A thin wrapper around a StateChangeObserver remote to allow for refcounting.
class StateChangeObserverWrapper
    : public base::RefCountedThreadSafe<StateChangeObserverWrapper> {
 public:
  explicit StateChangeObserverWrapper(
      std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer)
      : observer_(std::move(observer)) {}

  void OnStateChange(const updater::UpdateService::UpdateState& update_state) {
    (*observer_)->OnStateChange(MakeMojoUpdateState(update_state));
  }

  void OnComplete(updater::UpdateService::Result result) {
    (*observer_)->OnComplete(static_cast<mojom::UpdateService::Result>(result));
  }

 private:
  friend class base::RefCountedThreadSafe<StateChangeObserverWrapper>;
  virtual ~StateChangeObserverWrapper() = default;
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer_;
};

// Binds a callback that forwards state change callbacks and the OnComplete
// callback to a StateChangeObserver.
[[nodiscard]] std::pair<UpdateService::StateChangeCallback,
                        UpdateService::Callback>
MakeStateChangeObserverCallbacks(
    std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer) {
  scoped_refptr<StateChangeObserverWrapper> wrapper =
      base::MakeRefCounted<StateChangeObserverWrapper>(std::move(observer));
  return {
      base::BindRepeating(&StateChangeObserverWrapper::OnStateChange, wrapper),
      base::BindOnce(&StateChangeObserverWrapper::OnComplete, wrapper)};
}

// TODO(crbug.com/1378742): Implement some form of validation.
static bool IsTrustedIPCEndpoint(base::ProcessId /*caller_pid*/) {
  return true;
}

}  // namespace

UpdateServiceStub::UpdateServiceStub(scoped_refptr<updater::UpdateService> impl,
                                     UpdaterScope scope)
    : server_(GetActiveDutySocketPath(scope)->MaybeAsASCII(),
              this,
              base::BindRepeating(&IsTrustedIPCEndpoint)),
      impl_(impl) {
  server_.set_disconnect_handler(base::BindRepeating(
      &UpdateServiceStub::OnClientDisconnected, base::Unretained(this)));
  server_.StartServer();
}

UpdateServiceStub::~UpdateServiceStub() = default;

void UpdateServiceStub::OnClientDisconnected() {
  VLOG(1) << "Receiver disconnected: " << server_.current_receiver();
}

void UpdateServiceStub::GetVersion(GetVersionCallback callback) {
  impl_->GetVersion(base::BindOnce(
      [](GetVersionCallback callback, const base::Version& version) {
        std::move(callback).Run(version.GetString());
      },
      std::move(callback)));
}

void UpdateServiceStub::FetchPolicies(FetchPoliciesCallback callback) {
  impl_->FetchPolicies(std::move(callback));
}

void UpdateServiceStub::RegisterApp(mojom::RegistrationRequestPtr request,
                                    RegisterAppCallback callback) {
  impl_->RegisterApp(MakeRegistrationRequest(request), std::move(callback));
}

void UpdateServiceStub::GetAppStates(GetAppStatesCallback callback) {
  impl_->GetAppStates(
      base::BindOnce([](const std::vector<updater::UpdateService::AppState>&
                            app_states) {
        std::vector<mojom::AppStatePtr> app_states_mojom;
        std::transform(app_states.begin(), app_states.end(),
                       std::back_inserter(app_states_mojom), &MakeMojoAppState);
        return app_states_mojom;
      }).Then(std::move(callback)));
}

void UpdateServiceStub::RunPeriodicTasks(RunPeriodicTasksCallback callback) {
  impl_->RunPeriodicTasks(std::move(callback));
}

void UpdateServiceStub::UpdateAll(UpdateAllCallback callback) {
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->UpdateAll(std::move(state_change_callback),
                   std::move(on_complete_callback));
}

void UpdateServiceStub::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    UpdateCallback callback) {
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->Update(app_id, install_data_index,
                static_cast<updater::UpdateService::Priority>(priority),
                static_cast<updater::UpdateService::PolicySameVersionUpdate>(
                    policy_same_version_update),
                std::move(state_change_callback),
                std::move(on_complete_callback));
}

void UpdateServiceStub::Install(mojom::RegistrationRequestPtr registration,
                                const std::string& client_install_data,
                                const std::string& install_data_index,
                                UpdateService::Priority priority,
                                InstallCallback callback) {
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->Install(MakeRegistrationRequest(registration), client_install_data,
                 install_data_index,
                 static_cast<updater::UpdateService::Priority>(priority),
                 std::move(state_change_callback),
                 std::move(on_complete_callback));
}

void UpdateServiceStub::CancelInstalls(const std::string& app_id) {
  impl_->CancelInstalls(app_id);
}

void UpdateServiceStub::RunInstaller(const std::string& app_id,
                                     const ::base::FilePath& installer_path,
                                     const std::string& install_args,
                                     const std::string& install_data,
                                     const std::string& install_settings,
                                     RunInstallerCallback callback) {
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings, std::move(state_change_callback),
                      std::move(on_complete_callback));
}

}  // namespace updater
