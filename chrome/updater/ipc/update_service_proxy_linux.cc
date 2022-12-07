// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_linux.h"

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
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}

namespace updater {
namespace {

// The maximum amount of time to poll the server's socket for a connection.
constexpr base::TimeDelta kConnectionTimeout = base::Seconds(3);

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

mojo::PlatformChannelEndpoint ConnectMojo(UpdaterScope scope, int tries) {
  if (tries == 1) {
    // Launch a server process.
    absl::optional<base::FilePath> updater = GetUpdaterExecutablePath(scope);
    if (updater) {
      base::CommandLine command(*updater);
      command.AppendSwitch(kServerSwitch);
      command.AppendSwitchASCII(kServerServiceSwitch,
                                kServerUpdateServiceSwitchValue);
      if (scope == UpdaterScope::kSystem) {
        command.AppendSwitch(kSystemSwitch);
      }
      command.AppendSwitch(kEnableLoggingSwitch);
      command.AppendSwitchASCII(kLoggingModuleSwitch,
                                kLoggingModuleSwitchValue);
      base::LaunchProcess(command, {});
    }
  }

  return named_mojo_ipc_server::ConnectToServer(
      GetUpdateServiceServerName(scope));
}

void Connect(
    UpdaterScope scope,
    int tries,
    base::Time deadline,
    base::OnceCallback<void(absl::optional<mojo::PlatformChannelEndpoint>)>
        connected_callback) {
  if (base::Time::Now() > deadline) {
    LOG(ERROR) << "Failed to connect to UpdateService remote. "
                  "Connection timed out.";
    std::move(connected_callback).Run(absl::nullopt);
    return;
  }
  mojo::PlatformChannelEndpoint endpoint = ConnectMojo(scope, tries);

  if (endpoint.is_valid()) {
    std::move(connected_callback).Run(std::move(endpoint));
    return;
  }

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&Connect, scope, tries + 1, deadline,
                     std::move(connected_callback)),
      base::Milliseconds(30 * tries));
}

}  // namespace

class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl> {
 public:
  explicit UpdateServiceProxyImpl(UpdaterScope scope) : scope_(scope) {}

  UpdateServiceProxyImpl(UpdaterScope scope,
                         std::unique_ptr<mojo::IsolatedConnection> connection,
                         mojo::Remote<mojom::UpdateService> remote)
      : connection_(std::move(connection)),
        remote_(std::move(remote)),
        scope_(scope) {
    remote_.set_disconnect_handler(base::BindOnce(
        &UpdateServiceProxyImpl::OnDisconnected, weak_factory_.GetWeakPtr()));
  }

  void GetVersion(base::OnceCallback<void(const base::Version&)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::GetVersionCallback wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(
                [](base::OnceCallback<void(const base::Version&)> callback,
                   const std::string& version) {
                  std::move(callback).Run(base::Version(version));
                },
                std::move(callback)),
            "");

    if (!remote_) {
      return;
    }

    remote_->GetVersion(std::move(wrapped_callback));
  }

  void FetchPolicies(base::OnceCallback<void(int)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::FetchPoliciesCallback wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                    kErrorMojoDisconnect);
    if (!remote_) {
      return;
    }

    remote_->FetchPolicies(std::move(wrapped_callback));
  }

  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::RegisterAppCallback wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                    kErrorMojoDisconnect);

    if (!remote_) {
      return;
    }

    remote_->RegisterApp(MakeRegistrationRequest(request),
                         std::move(wrapped_callback));
  }

  void GetAppStates(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::GetAppStatesCallback wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce([](std::vector<mojom::AppStatePtr> app_states_mojo) {
              std::vector<updater::UpdateService::AppState> app_states;
              base::ranges::transform(app_states_mojo,
                                      std::back_inserter(app_states),
                                      &MakeAppState);
              return app_states;
            }).Then(std::move(callback)),
            std::vector<mojom::AppStatePtr>());

    if (!remote_) {
      return;
    }

    remote_->GetAppStates(std::move(wrapped_callback));
  }

  void RunPeriodicTasks(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::RunPeriodicTasksCallback wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback));

    if (!remote_) {
      return;
    }

    remote_->RunPeriodicTasks(std::move(wrapped_callback));
  }

  void UpdateAll(UpdateService::StateChangeCallback state_change_callback,
                 UpdateService::Callback complete_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::UpdateAllCallback state_change_observer_callback =
        MakeStateChangeObserver(std::move(state_change_callback),
                                std::move(complete_callback));
    if (!remote_) {
      return;
    }

    remote_->UpdateAll(std::move(state_change_observer_callback));
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              UpdateService::StateChangeCallback state_change_callback,
              UpdateService::Callback complete_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::UpdateCallback state_change_observer_callback =
        MakeStateChangeObserver(std::move(state_change_callback),
                                std::move(complete_callback));

    if (!remote_) {
      return;
    }

    remote_->Update(app_id, install_data_index,
                    static_cast<mojom::UpdateService::Priority>(priority),
                    static_cast<mojom::UpdateService::PolicySameVersionUpdate>(
                        policy_same_version_update),
                    std::move(state_change_observer_callback));
  }

  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               UpdateService::Priority priority,
               UpdateService::StateChangeCallback state_change_callback,
               UpdateService::Callback complete_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::InstallCallback state_change_observer_callback =
        MakeStateChangeObserver(std::move(state_change_callback),
                                std::move(complete_callback));

    if (!remote_) {
      return;
    }

    remote_->Install(MakeRegistrationRequest(registration), client_install_data,
                     install_data_index,
                     static_cast<mojom::UpdateService::Priority>(priority),
                     std::move(state_change_observer_callback));
  }

  void CancelInstalls(const std::string& app_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!remote_) {
      return;
    }

    remote_->CancelInstalls(app_id);
  }

  void RunInstaller(const std::string& app_id,
                    const base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    UpdateService::StateChangeCallback state_change_callback,
                    UpdateService::Callback complete_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    mojom::UpdateService::RunInstallerCallback state_change_observer_callback =
        MakeStateChangeObserver(std::move(state_change_callback),
                                std::move(complete_callback));

    if (!remote_) {
      return;
    }

    remote_->RunInstaller(app_id, installer_path, install_args, install_data,
                          install_settings,
                          std::move(state_change_observer_callback));
  }

  void EnsureConnecting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (remote_) {
      return;
    }

    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&Connect, scope_, 0,
                       base::Time::Now() + kConnectionTimeout,
                       OnCurrentSequence(base::BindOnce(
                           &UpdateServiceProxyImpl::OnConnected, this,
                           remote_.BindNewPipeAndPassReceiver()))));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImpl>;
  virtual ~UpdateServiceProxyImpl() = default;

  void OnDisconnected() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LOG(ERROR) << "UpdateService remote has unexpectedly disconnected.";
    connection_.reset();
    remote_.reset();
  }

  void OnConnected(mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
                   absl::optional<mojo::PlatformChannelEndpoint> endpoint) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!endpoint) {
      remote_.reset();
      return;
    }

    auto connection = std::make_unique<mojo::IsolatedConnection>();
    // Connect `remote_` to the RPC server by fusing its message pipe to the one
    // created by `IsolatedConnection::Connect`.
    if (!mojo::FusePipes(
            std::move(pending_receiver),
            mojo::PendingRemote<mojom::UpdateService>(
                connection->Connect(std::move(endpoint.value())), 0))) {
      LOG(ERROR) << "Failed to fuse Mojo pipes for RPC.";
      remote_.reset();
      return;
    }

    connection_ = std::move(connection);
    remote_.set_disconnect_handler(base::BindOnce(
        &UpdateServiceProxyImpl::OnDisconnected, weak_factory_.GetWeakPtr()));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateService> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const UpdaterScope scope_;

  base::WeakPtrFactory<UpdateServiceProxyImpl> weak_factory_{this};
};

UpdateServiceProxy::UpdateServiceProxy(UpdaterScope scope)
    : impl_(base::MakeRefCounted<UpdateServiceProxyImpl>(scope)) {}

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
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->GetVersion(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->FetchPolicies(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->RegisterApp(request, OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->GetAppStates(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->RunPeriodicTasks(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
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
  EnsureConnecting();
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
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->Install(registration, client_install_data, install_data_index,
                 priority, OnCurrentSequence(state_update),
                 OnCurrentSequence(std::move(callback)));
}

void UpdateServiceProxy::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->CancelInstalls(app_id);
}

void UpdateServiceProxy::RunInstaller(const std::string& app_id,
                                      const base::FilePath& installer_path,
                                      const std::string& install_args,
                                      const std::string& install_data,
                                      const std::string& install_settings,
                                      StateChangeCallback state_update,
                                      Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  VLOG(1) << __func__;
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings, OnCurrentSequence(state_update),
                      OnCurrentSequence(std::move(callback)));
}

UpdateServiceProxy::~UpdateServiceProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
}

void UpdateServiceProxy::EnsureConnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->EnsureConnecting();
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    const base::TimeDelta& timeout) {
  return base::MakeRefCounted<UpdateServiceProxy>(scope);
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateService> remote) {
  return base::MakeRefCounted<UpdateServiceProxy>(scope, std::move(connection),
                                                  std::move(remote));
}

}  // namespace updater
