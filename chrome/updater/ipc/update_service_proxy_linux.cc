// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_linux.h"
#include "base/threading/platform_thread.h"
#include "chrome/updater/service_proxy_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/linux/mojom/updater_service.mojom.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}

namespace updater {
namespace {

[[nodiscard]] UpdateService::UpdateState MakeUpdateState(
    const mojom::UpdateStatePtr& state_mojom) {
  updater::UpdateService::UpdateState state;
  state.app_id = state_mojom->app_id;
  state.state =
      static_cast<UpdateService::UpdateState::State>(state_mojom->state);
  state.next_version = base::Version(state_mojom->next_version);
  state.downloaded_bytes = state_mojom->downloaded_bytes;
  state.total_bytes = state_mojom->total_bytes;
  state.install_progress = state_mojom->install_progress;
  state.error_category =
      static_cast<UpdateService::ErrorCategory>(state_mojom->error_category);
  state.error_code = state_mojom->error_code;
  state.extra_code1 = state_mojom->extra_code1;
  state.installer_text = state_mojom->installer_text;
  state.installer_cmd_line = state_mojom->installer_cmd_line;

  return state;
}

[[nodiscard]] UpdateService::AppState MakeAppState(
    const mojom::AppStatePtr& app_state_mojo) {
  UpdateService::AppState app_state;
  app_state.app_id = app_state_mojo->app_id;
  app_state.version = base::Version(app_state_mojo->version);
  app_state.ap = app_state_mojo->ap;
  app_state.brand_code = app_state_mojo->brand_code;
  app_state.brand_path = app_state_mojo->brand_path;
  app_state.ecp = app_state_mojo->ecp;

  return app_state;
}

[[nodiscard]] mojom::RegistrationRequestPtr MakeRegistrationRequest(
    const RegistrationRequest& request) {
  return mojom::RegistrationRequest::New(
      request.app_id, request.brand_code, request.brand_path, request.ap,
      request.version.GetString(), request.existence_checker_path);
}

class StateChangeObserverImpl : public mojom::StateChangeObserver {
 public:
  explicit StateChangeObserverImpl(
      UpdateService::StateChangeCallback state_change_callback,
      UpdateService::Callback complete_callback)
      : state_change_callback_(std::move(state_change_callback)),
        complete_callback_(std::move(complete_callback)) {}
  StateChangeObserverImpl(const StateChangeObserverImpl&) = delete;
  StateChangeObserverImpl& operator=(const StateChangeObserverImpl&) = delete;
  ~StateChangeObserverImpl() override = default;

  // Overrides for mojom::StateChangeObserver.
  void OnStateChange(mojom::UpdateStatePtr state_mojom) override {
    DCHECK(complete_callback_) << "OnStateChange received after OnComplete";
    state_change_callback_.Run(MakeUpdateState(state_mojom));
  }

  void OnComplete(mojom::UpdateService::Result result) override {
    DCHECK(complete_callback_) << "OnComplete received without a valid "
                                  "callback. Was OnComplete run twice?";
    if (complete_callback_) {
      std::move(complete_callback_)
          .Run(static_cast<updater::UpdateService::Result>(result));
    }
  }

 private:
  UpdateService::StateChangeCallback state_change_callback_;
  UpdateService::Callback complete_callback_;
};

// Binds a callback which creates a self-owned StateChangeObserverImpl to
// forward RPC callbacks to the provided native callbacks.
[[nodiscard]] base::OnceCallback<
    void(mojo::PendingReceiver<mojom::StateChangeObserver>)>
MakeStateChangeObserver(
    UpdateService::StateChangeCallback state_change_callback,
    UpdateService::Callback complete_callback) {
  complete_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(complete_callback),
      updater::UpdateService::Result::kIPCConnectionFailed);
  return base::BindOnce(
      [](UpdateService::StateChangeCallback state_change_callback,
         UpdateService::Callback complete_callback,
         mojo::PendingReceiver<mojom::StateChangeObserver> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<StateChangeObserverImpl>(
                std::move(state_change_callback), std::move(complete_callback)),
            std::move(receiver));
      },
      std::move(state_change_callback), std::move(complete_callback));
}

}  // namespace

class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl> {
 public:
  explicit UpdateServiceProxyImpl(
      UpdaterScope /*scope*/,
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<mojom::UpdateService> remote)
      : connection_(std::move(connection)), remote_(std::move(remote)) {}

  void GetVersion(base::OnceCallback<void(const base::Version&)> callback) {
    remote_->GetVersion(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(
            [](base::OnceCallback<void(const base::Version&)> callback,
               const std::string& version) {
              std::move(callback).Run(base::Version(version));
            },
            std::move(callback)),
        ""));
  }

  void FetchPolicies(base::OnceCallback<void(int)> callback) {
    remote_->FetchPolicies(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        std::move(callback), kErrorMojoDisconnect));
  }

  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) {
    remote_->RegisterApp(MakeRegistrationRequest(request),
                         mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                             std::move(callback), kErrorMojoDisconnect));
  }

  void GetAppStates(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
          callback) {
    remote_->GetAppStates(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce([](std::vector<mojom::AppStatePtr> app_states_mojo) {
          std::vector<updater::UpdateService::AppState> app_states;
          base::ranges::transform(
              app_states_mojo, std::back_inserter(app_states), &MakeAppState);
          return app_states;
        }).Then(std::move(callback)),
        std::vector<mojom::AppStatePtr>()));
  }

  void RunPeriodicTasks(base::OnceClosure callback) {
    remote_->RunPeriodicTasks(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
  }

  void UpdateAll(UpdateService::StateChangeCallback state_change_callback,
                 UpdateService::Callback complete_callback) {
    remote_->UpdateAll(MakeStateChangeObserver(std::move(state_change_callback),
                                               std::move(complete_callback)));
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              UpdateService::StateChangeCallback state_change_callback,
              UpdateService::Callback complete_callback) {
    remote_->Update(app_id, install_data_index,
                    static_cast<mojom::UpdateService::Priority>(priority),
                    static_cast<mojom::UpdateService::PolicySameVersionUpdate>(
                        policy_same_version_update),
                    MakeStateChangeObserver(std::move(state_change_callback),
                                            std::move(complete_callback)));
  }

  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               UpdateService::Priority priority,
               UpdateService::StateChangeCallback state_change_callback,
               UpdateService::Callback complete_callback) {
    remote_->Install(MakeRegistrationRequest(registration), client_install_data,
                     install_data_index,
                     static_cast<mojom::UpdateService::Priority>(priority),
                     MakeStateChangeObserver(std::move(state_change_callback),
                                             std::move(complete_callback)));
  }

  void CancelInstalls(const std::string& app_id) {
    remote_->CancelInstalls(app_id);
  }

  void RunInstaller(const std::string& app_id,
                    const base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    UpdateService::StateChangeCallback state_change_callback,
                    UpdateService::Callback complete_callback) {
    remote_->RunInstaller(
        app_id, installer_path, install_args, install_data, install_settings,
        MakeStateChangeObserver(std::move(state_change_callback),
                                std::move(complete_callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImpl>;
  virtual ~UpdateServiceProxyImpl() = default;

  std::unique_ptr<mojo::IsolatedConnection> connection_;
  mojo::Remote<mojom::UpdateService> remote_;
};

UpdateServiceProxy::UpdateServiceProxy(
    UpdaterScope updater_scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateService> remote)
    : impl_(base::MakeRefCounted<UpdateServiceProxyImpl>(updater_scope,
                                                         std::move(connection),
                                                         std::move(remote))) {}

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->GetVersion(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->FetchPolicies(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RegisterApp(request, OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->GetAppStates(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RunPeriodicTasks(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->UpdateAll(OnCurrentSequence(state_update),
                   OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Update(app_id, install_data_index, priority,
                policy_same_version_update, OnCurrentSequence(state_update),
                OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::Install(const RegistrationRequest& registration,
                                 const std::string& client_install_data,
                                 const std::string& install_data_index,
                                 Priority priority,
                                 StateChangeCallback state_update,
                                 Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Install(registration, client_install_data, install_data_index,
                 priority, OnCurrentSequence(state_update),
                 OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
}

void UpdateServiceProxy::RunInstaller(const std::string& app_id,
                                      const base::FilePath& installer_path,
                                      const std::string& install_args,
                                      const std::string& install_data,
                                      const std::string& install_settings,
                                      StateChangeCallback state_update,
                                      Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings, OnCurrentSequence(state_update),
                      OnCurrentSequence(std::move(callback)));
}

// TODO(crbug.com/1363829) - remove the function.
void UpdateServiceProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

UpdateServiceProxy::~UpdateServiceProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    const base::TimeDelta& timeout) {
  absl::optional<base::FilePath> socket_path = GetActiveDutySocketPath(scope);
  if (!socket_path)
    return nullptr;

  mojo::PlatformChannelEndpoint endpoint;

  // TODO(1382127): Avoid blocking the calling thread.
  base::Time deadline = base::Time::NowFromSystemTime() + timeout;
  do {
    endpoint = mojo::NamedPlatformChannel::ConnectToServer(
        socket_path->MaybeAsASCII());
    base::PlatformThread::Sleep(base::Milliseconds(100));
  } while (!endpoint.is_valid() && base::Time::NowFromSystemTime() < deadline);

  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Failed to connect to UpdateService remote. Connection timed "
                  "out.";
    return nullptr;
  }

  auto connection = std::make_unique<mojo::IsolatedConnection>();

  mojo::Remote<mojom::UpdateService> remote(
      mojo::PendingRemote<mojom::UpdateService>(
          connection->Connect(std::move(endpoint)), 0));
  remote.set_disconnect_handler(base::BindOnce([]() {
    LOG(ERROR) << "UpdateService remote has unexpectedly disconnected.";
  }));

  return CreateUpdateServiceProxy(scope, std::move(connection),
                                  std::move(remote));
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateService> remote) {
  return base::MakeRefCounted<UpdateServiceProxy>(scope, std::move(connection),
                                                  std::move(remote));
}

}  // namespace updater
