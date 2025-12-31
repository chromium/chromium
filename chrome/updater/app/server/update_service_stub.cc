// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/update_service_stub.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/ipc_security.h"
#include "chrome/updater/mojom/updater_service.mojom-forward.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace updater {
namespace {

[[nodiscard]] mojom::AppStatePtr MakeMojoAppState(
    const updater::UpdateService::AppState& app_state) {
  return mojom::AppState::New(app_state);
}

[[nodiscard]] mojom::UpdateStatePtr MakeMojoUpdateState(
    const updater::UpdateService::UpdateState& update_state) {
  return mojom::UpdateState::New(update_state);
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
[[nodiscard]] std::pair<
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>,
    base::OnceCallback<void(UpdateService::Result)>>
MakeStateChangeObserverCallbacks(
    std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer) {
  auto wrapper =
      base::MakeRefCounted<StateChangeObserverWrapper>(std::move(observer));
  return {
      base::BindRepeating(&StateChangeObserverWrapper::OnStateChange, wrapper),
      base::BindOnce(&StateChangeObserverWrapper::OnComplete, wrapper)};
}

// UpdateServiceStubUntrusted only forwards certain safe calls to its underlying
// UpdateService.
class UpdateServiceStubUntrusted : public mojom::UpdateService {
 public:
  // `impl` must outlive `this`, since it `impl` is saved as a raw pointer.
  explicit UpdateServiceStubUntrusted(mojom::UpdateService* impl)
      : impl_(impl) {}
  UpdateServiceStubUntrusted(const UpdateServiceStubUntrusted&) = delete;
  UpdateServiceStubUntrusted& operator=(const UpdateServiceStubUntrusted&) =
      delete;

  // updater::mojom::UpdateService
  void GetVersion(GetVersionCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->GetVersion(std::move(callback));
  }

  void GetAppStates(GetAppStatesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->GetAppStates(std::move(callback));
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              bool do_update_check_only,
              const std::optional<std::string>& language,
              UpdateCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->Update(app_id, install_data_index, priority,
                  policy_same_version_update, do_update_check_only,
                  language.value_or(""), std::move(callback));
  }

  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      const std::optional<std::string>& language,
      UpdateCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->CheckForUpdate(app_id, priority, policy_same_version_update,
                          language.value_or(""), std::move(callback));
  }

  void RunPeriodicTasks(RunPeriodicTasksCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->RunPeriodicTasks(std::move(callback));
  }

  void FetchPolicies(policy::PolicyFetchReason reason,
                     FetchPoliciesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->FetchPolicies(reason, std::move(callback));
  }

  void UpdateAll(UpdateAllCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->UpdateAll(std::move(callback));
  }

  // The rest of updater::mojom::UpdateService is rejected.
  void RegisterApp(mojom::RegistrationRequestPtr request,
                   RegisterAppCallback callback) override {
    VLOG(1) << __func__ << " rejected (untrusted caller)";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(kErrorPermissionDenied);
  }

  void Install(mojom::RegistrationRequestPtr registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               UpdateService::Priority priority,
               const std::optional<std::string>& language,
               InstallCallback callback) override {
    VLOG(1) << __func__ << " rejected (untrusted caller)";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::Remote<mojom::StateChangeObserver> observer;
    std::move(callback).Run(observer.BindNewPipeAndPassReceiver());
    observer->OnComplete(mojom::UpdateService_Result::kPermissionDenied);
  }

  void CancelInstalls(const std::string& app_id) override {
    VLOG(1) << __func__ << " rejected (untrusted caller)";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void RunInstaller(const std::string& app_id,
                    const ::base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    const std::optional<std::string>& language,
                    RunInstallerCallback callback) override {
    VLOG(1) << __func__ << " rejected (untrusted caller)";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::Remote<mojom::StateChangeObserver> observer;
    std::move(callback).Run(observer.BindNewPipeAndPassReceiver());
    observer->OnComplete(mojom::UpdateService_Result::kPermissionDenied);
  }

 private:
  void OnClientDisconnected();

  raw_ptr<mojom::UpdateService> impl_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

UpdateServiceStub::UpdateServiceStub(scoped_refptr<updater::UpdateService> impl,
                                     UpdaterScope scope,
                                     base::RepeatingClosure task_start_listener,
                                     base::RepeatingClosure task_end_listener)
    : UpdateServiceStub(impl,
                        scope,
                        task_start_listener,
                        task_end_listener,
                        base::RepeatingClosure()) {}

UpdateServiceStub::~UpdateServiceStub() = default;

void UpdateServiceStub::GetVersion(GetVersionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->GetVersion(
      base::BindOnce(
          [](GetVersionCallback callback, const base::Version& version) {
            std::move(callback).Run(version.GetString());
          },
          std::move(callback))
          .Then(task_end_listener_));
}

void UpdateServiceStub::FetchPolicies(policy::PolicyFetchReason reason,
                                      FetchPoliciesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->FetchPolicies(reason, std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStub::RegisterApp(mojom::RegistrationRequestPtr request,
                                    RegisterAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  CHECK(request);
  impl_->RegisterApp(*request, std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStub::GetAppStates(GetAppStatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->GetAppStates(
      base::BindOnce(
          [](const std::vector<updater::UpdateService::AppState>& app_states) {
            std::vector<mojom::AppStatePtr> app_states_mojom;
            std::ranges::transform(app_states,
                                   std::back_inserter(app_states_mojom),
                                   &MakeMojoAppState);
            return app_states_mojom;
          })
          .Then(std::move(callback))
          .Then(task_end_listener_));
}

void UpdateServiceStub::RunPeriodicTasks(RunPeriodicTasksCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->RunPeriodicTasks(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStub::UpdateAll(UpdateAllCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->UpdateAll(std::move(state_change_callback),
                   std::move(on_complete_callback).Then(task_end_listener_));
}

void UpdateServiceStub::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    bool do_update_check_only,
    const std::optional<std::string>& language,
    UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  auto observer = std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  if (do_update_check_only) {
    impl_->CheckForUpdate(
        app_id, static_cast<updater::UpdateService::Priority>(priority),
        static_cast<updater::UpdateService::PolicySameVersionUpdate>(
            policy_same_version_update),
        language.value_or(""), state_change_callback,
        std::move(on_complete_callback).Then(task_end_listener_));
  } else {
    impl_->Update(app_id, install_data_index,
                  static_cast<updater::UpdateService::Priority>(priority),
                  static_cast<updater::UpdateService::PolicySameVersionUpdate>(
                      policy_same_version_update),
                  language.value_or(""), state_change_callback,
                  std::move(on_complete_callback).Then(task_end_listener_));
  }
}

void UpdateServiceStub::Install(mojom::RegistrationRequestPtr registration,
                                const std::string& client_install_data,
                                const std::string& install_data_index,
                                UpdateService::Priority priority,
                                const std::optional<std::string>& language,
                                InstallCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  CHECK(registration);
  impl_->Install(*registration, client_install_data, install_data_index,
                 static_cast<updater::UpdateService::Priority>(priority),
                 language.value_or(""), std::move(state_change_callback),
                 std::move(on_complete_callback).Then(task_end_listener_));
}

void UpdateServiceStub::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->CancelInstalls(app_id);
  task_end_listener_.Run();
}

void UpdateServiceStub::RunInstaller(const std::string& app_id,
                                     const ::base::FilePath& installer_path,
                                     const std::string& install_args,
                                     const std::string& install_data,
                                     const std::string& install_settings,
                                     const std::optional<std::string>& language,
                                     RunInstallerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings, language.value_or(""),
                      std::move(state_change_callback),
                      std::move(on_complete_callback).Then(task_end_listener_));
}

void UpdateServiceStub::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    const std::optional<std::string>& language,
    UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  auto observer = std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());
  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->CheckForUpdate(
      app_id, static_cast<updater::UpdateService::Priority>(priority),
      static_cast<updater::UpdateService::PolicySameVersionUpdate>(
          policy_same_version_update),
      language.value_or(""), state_change_callback,
      std::move(on_complete_callback).Then(task_end_listener_));
}

UpdateServiceStub::UpdateServiceStub(
    scoped_refptr<updater::UpdateService> impl,
    UpdaterScope scope,
    base::RepeatingClosure task_start_listener,
    base::RepeatingClosure task_end_listener,
    base::RepeatingClosure endpoint_created_listener_for_testing)
    : filter_(std::make_unique<UpdateServiceStubUntrusted>(this)),
      server_(CreateServerEndpointOptions(GetUpdateServiceServerName(scope)),
              base::BindRepeating(base::BindRepeating(
                  [](mojom::UpdateService* interface,
                     mojom::UpdateService* filter,
                     const named_mojo_ipc_server::ConnectionInfo& info) {
                    return IsConnectionTrusted(info) ? interface : filter;
                  },
                  this,
                  filter_.get()))),
      impl_(impl),
      task_start_listener_(task_start_listener),
      task_end_listener_(task_end_listener) {
  server_.set_disconnect_handler(base::BindRepeating(
      [] { VLOG(1) << "UpdateService client disconnected."; }));
  if (endpoint_created_listener_for_testing) {
    server_.set_on_server_endpoint_created_callback_for_testing(  // IN-TEST
        endpoint_created_listener_for_testing);
  }
  server_.StartServer();
}

}  // namespace updater
