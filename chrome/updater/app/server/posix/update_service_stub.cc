// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/posix/update_service_stub.h"

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom-forward.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/ipc_security.h"
#include "chrome/updater/registration_data.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
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
  if (mojom->version_path) {
    request.version_path = *mojom->version_path;
  }
  if (mojom->version_key) {
    request.version_key = *mojom->version_key;
  }
  if (mojom->ap_path) {
    request.ap_path = *mojom->ap_path;
  }
  if (mojom->ap_key) {
    request.ap_key = *mojom->ap_key;
  }
  return request;
}

[[nodiscard]] mojom::AppStatePtr MakeMojoAppState(
    const updater::UpdateService::AppState& app_state) {
  return mojom::AppState::New(
      app_state.app_id, app_state.version.GetString(), app_state.ap,
      app_state.brand_code, app_state.brand_path, app_state.ecp,
      app_state.ap_path, app_state.ap_key, app_state.version_path,
      app_state.version_key, app_state.cohort);
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
  ~UpdateServiceStubUntrusted() override = default;

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
              UpdateCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->Update(app_id, install_data_index, priority,
                  policy_same_version_update, do_update_check_only,
                  std::move(callback));
  }

  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      UpdateCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->CheckForUpdate(app_id, priority, policy_same_version_update,
                          std::move(callback));
  }

  void RunPeriodicTasks(RunPeriodicTasksCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->RunPeriodicTasks(std::move(callback));
  }

  void FetchPolicies(FetchPoliciesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->FetchPolicies(std::move(callback));
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

void UpdateServiceStub::FetchPolicies(FetchPoliciesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->FetchPolicies(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStub::RegisterApp(mojom::RegistrationRequestPtr request,
                                    RegisterAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->RegisterApp(MakeRegistrationRequest(request),
                     std::move(callback).Then(task_end_listener_));
}

void UpdateServiceStub::GetAppStates(GetAppStatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->GetAppStates(
      base::BindOnce(
          [](const std::vector<updater::UpdateService::AppState>& app_states) {
            std::vector<mojom::AppStatePtr> app_states_mojom;
            base::ranges::transform(app_states,
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
        state_change_callback,
        std::move(on_complete_callback).Then(task_end_listener_));
  } else {
    impl_->Update(app_id, install_data_index,
                  static_cast<updater::UpdateService::Priority>(priority),
                  static_cast<updater::UpdateService::PolicySameVersionUpdate>(
                      policy_same_version_update),
                  state_change_callback,
                  std::move(on_complete_callback).Then(task_end_listener_));
  }
}

void UpdateServiceStub::Install(mojom::RegistrationRequestPtr registration,
                                const std::string& client_install_data,
                                const std::string& install_data_index,
                                UpdateService::Priority priority,
                                InstallCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->Install(MakeRegistrationRequest(registration), client_install_data,
                 install_data_index,
                 static_cast<updater::UpdateService::Priority>(priority),
                 std::move(state_change_callback),
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
                                     RunInstallerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  std::unique_ptr<mojo::Remote<mojom::StateChangeObserver>> observer =
      std::make_unique<mojo::Remote<mojom::StateChangeObserver>>();
  std::move(callback).Run(observer->BindNewPipeAndPassReceiver());

  auto [state_change_callback, on_complete_callback] =
      MakeStateChangeObserverCallbacks(std::move(observer));
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings, std::move(state_change_callback),
                      std::move(on_complete_callback).Then(task_end_listener_));
}

void UpdateServiceStub::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
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
      state_change_callback,
      std::move(on_complete_callback).Then(task_end_listener_));
}

UpdateServiceStub::UpdateServiceStub(
    scoped_refptr<updater::UpdateService> impl,
    UpdaterScope scope,
    base::RepeatingClosure task_start_listener,
    base::RepeatingClosure task_end_listener,
    base::RepeatingClosure endpoint_created_listener_for_testing)
    : filter_(std::make_unique<UpdateServiceStubUntrusted>(this)),
      server_({GetUpdateServiceServerName(scope),
               named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection},
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
