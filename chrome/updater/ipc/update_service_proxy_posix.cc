// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_posix.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/update_service_dialer.h"
#include "chrome/updater/ipc/update_service_proxy.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/posix_util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace updater {
namespace {

// The maximum amount of time to poll the server's socket for a connection.
constexpr base::TimeDelta kConnectionTimeout = base::Minutes(3);

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
  if (app_state_mojo->version_path) {
    app_state.version_path = *app_state_mojo->version_path;
  }
  if (app_state_mojo->version_key) {
    app_state.version_key = *app_state_mojo->version_key;
  }
  app_state.ap = app_state_mojo->ap;
  if (app_state_mojo->ap_path) {
    app_state.ap_path = *app_state_mojo->ap_path;
  }
  if (app_state_mojo->ap_key) {
    app_state.ap_key = *app_state_mojo->ap_key;
  }
  app_state.brand_code = app_state_mojo->brand_code;
  app_state.brand_path = app_state_mojo->brand_path;
  app_state.ecp = app_state_mojo->ecp;
  if (app_state_mojo->cohort) {
    app_state.cohort = *app_state_mojo->cohort;
  }

  return app_state;
}

[[nodiscard]] mojom::RegistrationRequestPtr MakeRegistrationRequest(
    const RegistrationRequest& request) {
  return mojom::RegistrationRequest::New(
      request.app_id, request.brand_code, request.brand_path, request.ap,
      request.version.GetString(), request.existence_checker_path,
      request.ap_path, request.ap_key, request.version_path,
      request.version_key);
}

class StateChangeObserverImpl : public mojom::StateChangeObserver {
 public:
  explicit StateChangeObserverImpl(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_change_callback,
      base::OnceCallback<void(UpdateService::Result)> complete_callback)
      : state_change_callback_(std::move(state_change_callback)),
        complete_callback_(std::move(complete_callback)) {}
  StateChangeObserverImpl(const StateChangeObserverImpl&) = delete;
  StateChangeObserverImpl& operator=(const StateChangeObserverImpl&) = delete;
  ~StateChangeObserverImpl() override = default;

  // Overrides for mojom::StateChangeObserver.
  void OnStateChange(mojom::UpdateStatePtr state_mojom) override {
    CHECK(complete_callback_) << "OnStateChange received after OnComplete";
    state_change_callback_.Run(MakeUpdateState(state_mojom));
  }

  void OnComplete(mojom::UpdateService::Result result) override {
    CHECK(complete_callback_) << "OnComplete received without a valid "
                                 "callback. Was OnComplete run twice?";
    if (complete_callback_) {
      std::move(complete_callback_)
          .Run(static_cast<updater::UpdateService::Result>(result));
    }
  }

 private:
  base::RepeatingCallback<void(const UpdateService::UpdateState&)>
      state_change_callback_;
  base::OnceCallback<void(UpdateService::Result)> complete_callback_;
};

template <typename T>
base::OnceCallback<void(T)> ToMojoCallback(
    base::OnceCallback<void(base::expected<T, RpcError>)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(base::expected<T, RpcError>)> callback,
         T value) { std::move(callback).Run(base::ok(value)); },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), base::unexpected(kErrorIpcDisconnect)));
}

// Binds a callback which creates a self-owned StateChangeObserverImpl to
// forward RPC callbacks to the provided native callbacks.
[[nodiscard]] base::OnceCallback<
    void(mojo::PendingReceiver<mojom::StateChangeObserver>)>
MakeStateChangeObserver(
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_change_callback,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        complete_callback) {
  return base::BindOnce(
      [](base::RepeatingCallback<void(const UpdateService::UpdateState&)>
             state_change_callback,
         base::OnceCallback<void(UpdateService::Result)> complete_callback,
         mojo::PendingReceiver<mojom::StateChangeObserver> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<StateChangeObserverImpl>(
                state_change_callback,
                base::BindOnce(std::move(complete_callback))),
            std::move(receiver));
      },
      base::BindPostTaskToCurrentDefault(state_change_callback),
      base::BindPostTaskToCurrentDefault(
          ToMojoCallback(std::move(complete_callback))));
}

std::optional<mojo::PlatformChannelEndpoint> ConnectMojo(UpdaterScope scope,
                                                         int tries) {
  if (tries == 1 && !DialUpdateService(scope)) {
    return std::nullopt;
  }
  return named_mojo_ipc_server::ConnectToServer(
      GetUpdateServiceServerName(scope));
}

void Connect(
    UpdaterScope scope,
    int tries,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>)>
        connected_callback) {
  if (base::Time::Now() > deadline) {
    VLOG(1) << "Failed to connect to UpdateService remote. "
               "Connection timed out.";
    std::move(connected_callback).Run(std::nullopt);
    return;
  }
  std::optional<mojo::PlatformChannelEndpoint> endpoint =
      ConnectMojo(scope, tries);

  if (!endpoint) {
    VLOG(1) << "Failed to connect to UpdateService remote. "
               "No updater exists.";
    std::move(connected_callback).Run(std::nullopt);
    return;
  }

  if (endpoint->is_valid()) {
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

UpdateServiceProxyImpl::UpdateServiceProxyImpl(
    UpdaterScope scope,
    base::TimeDelta get_version_timeout)
    : scope_(scope), get_version_timeout_(get_version_timeout) {}

void UpdateServiceProxyImpl::GetVersion(
    base::OnceCallback<void(base::expected<base::Version, RpcError>)>
        callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();

  // Because GetVersion is used as a health checker, it has a special timeout
  // in the event that the server receives the call and hangs. If the timeout
  // elapses, this calls OnDisconnected to reset the connection and trigger the
  // DefaultInvokeIfNotRun wrapper around `callback`.
  auto timeout_callback =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          &UpdateServiceProxyImpl::OnDisconnected, weak_factory_.GetWeakPtr()));

  // If get_version_timeout_ elapses, call the timeout callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_callback->callback(), get_version_timeout_);

  remote_->GetVersion(base::BindOnce(
      [](base::OnceCallback<void(base::expected<base::Version, RpcError>)>
             callback,
         std::unique_ptr<base::CancelableOnceClosure> timeout_callback,
         const std::string& version) {
        timeout_callback->Cancel();
        std::move(callback).Run(base::Version(version));
      },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), base::unexpected(kErrorIpcDisconnect)),
      std::move(timeout_callback)));
}

void UpdateServiceProxyImpl::FetchPolicies(
    base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->FetchPolicies(ToMojoCallback(std::move(callback)));
}

void UpdateServiceProxyImpl::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->RegisterApp(MakeRegistrationRequest(request),
                       ToMojoCallback(std::move(callback)));
}

void UpdateServiceProxyImpl::GetAppStates(
    base::OnceCallback<void(base::expected<std::vector<UpdateService::AppState>,
                                           RpcError>)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->GetAppStates(
      base::BindOnce([](std::vector<mojom::AppStatePtr> app_states_mojo) {
        std::vector<updater::UpdateService::AppState> app_states;
        base::ranges::transform(app_states_mojo, std::back_inserter(app_states),
                                &MakeAppState);
        return app_states;
      }).Then(ToMojoCallback(std::move(callback))));
}

void UpdateServiceProxyImpl::RunPeriodicTasks(
    base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->RunPeriodicTasks(base::BindOnce(
      [](base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kErrorOk);
      },
      ToMojoCallback(std::move(callback))));
}

void UpdateServiceProxyImpl::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->CheckForUpdate(
      app_id, static_cast<mojom::UpdateService::Priority>(priority),
      static_cast<mojom::UpdateService::PolicySameVersionUpdate>(
          policy_same_version_update),
      MakeStateChangeObserver(state_update, std::move(callback)));
}

void UpdateServiceProxyImpl::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->Update(app_id, install_data_index,
                  static_cast<mojom::UpdateService::Priority>(priority),
                  static_cast<mojom::UpdateService::PolicySameVersionUpdate>(
                      policy_same_version_update),
                  /*do_update_check_only=*/false,
                  MakeStateChangeObserver(state_update, std::move(callback)));
}

void UpdateServiceProxyImpl::UpdateAll(
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->UpdateAll(
      MakeStateChangeObserver(state_update, std::move(callback)));
}

void UpdateServiceProxyImpl::Install(
    const RegistrationRequest& registration,
    const std::string& client_install_data,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->Install(MakeRegistrationRequest(registration), client_install_data,
                   install_data_index,
                   static_cast<mojom::UpdateService::Priority>(priority),
                   MakeStateChangeObserver(state_update, std::move(callback)));
}

void UpdateServiceProxyImpl::CancelInstalls(const std::string& app_id) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->CancelInstalls(app_id);
}

void UpdateServiceProxyImpl::RunInstaller(
    const std::string& app_id,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    const std::string& install_settings,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->RunInstaller(
      app_id, installer_path, install_args, install_data, install_settings,
      MakeStateChangeObserver(state_update, std::move(callback)));
}

void UpdateServiceProxyImpl::OnConnected(
    mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
    std::optional<mojo::PlatformChannelEndpoint> endpoint) {
  VLOG(1) << __func__;
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

  // A weak pointer is used here to prevent remote_ from forming a reference
  // cycle with this object.
  remote_.set_disconnect_handler(base::BindOnce(
      &UpdateServiceProxyImpl::OnDisconnected, weak_factory_.GetWeakPtr()));
}

void UpdateServiceProxyImpl::OnDisconnected() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.reset();
  remote_.reset();
}

UpdateServiceProxyImpl::~UpdateServiceProxyImpl() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpdateServiceProxyImpl::EnsureConnecting() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &Connect, scope_, 0, base::Time::Now() + kConnectionTimeout,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &UpdateServiceProxyImpl::OnConnected, weak_factory_.GetWeakPtr(),
              remote_.BindNewPipeAndPassReceiver()))));
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(UpdaterScope scope,
                                                      base::TimeDelta timeout) {
  return base::MakeRefCounted<UpdateServiceProxy>(
      base::MakeRefCounted<UpdateServiceProxyImpl>(scope, timeout));
}

}  // namespace updater
