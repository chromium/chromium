// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_posix.h"

#include <memory>
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
#include "base/version.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/update_service_dialer.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  return base::BindOnce(
      [](UpdateService::StateChangeCallback state_change_callback,
         UpdateService::Callback complete_callback,
         mojo::PendingReceiver<mojom::StateChangeObserver> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<StateChangeObserverImpl>(
                state_change_callback, std::move(complete_callback)),
            std::move(receiver));
      },
      state_change_callback,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(complete_callback),
          updater::UpdateService::Result::kIPCConnectionFailed));
}

absl::optional<mojo::PlatformChannelEndpoint> ConnectMojo(UpdaterScope scope,
                                                          int tries) {
  if (tries == 1) {
    if (!DialUpdateService(scope)) {
      return absl::nullopt;
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
    VLOG(1) << "Failed to connect to UpdateService remote. "
               "Connection timed out.";
    std::move(connected_callback).Run(absl::nullopt);
    return;
  }
  absl::optional<mojo::PlatformChannelEndpoint> endpoint =
      ConnectMojo(scope, tries);

  if (!endpoint) {
    VLOG(1) << "Failed to connect to UpdateService remote. "
               "No updater exists.";
    std::move(connected_callback).Run(absl::nullopt);
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

UpdateServiceProxy::UpdateServiceProxy(
    UpdaterScope scope,
    const base::TimeDelta& get_version_timeout)
    : scope_(scope), get_version_timeout_(get_version_timeout) {}

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();

  // Because GetVersion is used as a health checker, it has a special timeout
  // in the event that the server receives the call and hangs. If the timeout
  // elapses, this calls OnDisconnected to reset the connection and trigger the
  // DefaultInvokeIfNotRun wrapper around `callback`.
  auto timeout_callback =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          &UpdateServiceProxy::OnDisconnected, weak_factory_.GetWeakPtr()));

  // If get_version_timeout_ elapses, call the timeout callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_callback->callback(), get_version_timeout_);

  base::OnceCallback<void(const std::string&)> combined_callback =
      base::BindOnce(
          [](base::OnceCallback<void(const base::Version&)> callback,
             std::unique_ptr<base::CancelableOnceClosure> timeout_callback,
             const std::string& version) {
            timeout_callback->Cancel();
            std::move(callback).Run(base::Version(version));
          },
          std::move(callback), std::move(timeout_callback));
  remote_->GetVersion(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(combined_callback), ""));
}

void UpdateServiceProxy::FetchPolicies(base::OnceCallback<void(int)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->FetchPolicies(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), kErrorMojoDisconnect));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     base::OnceCallback<void(int)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->RegisterApp(MakeRegistrationRequest(request),
                       mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                           std::move(callback), kErrorMojoDisconnect));
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::GetAppStatesCallback wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce([](std::vector<mojom::AppStatePtr> app_states_mojo) {
            std::vector<updater::UpdateService::AppState> app_states;
            base::ranges::transform(
                app_states_mojo, std::back_inserter(app_states), &MakeAppState);
            return app_states;
          }).Then(std::move(callback)),
          std::vector<mojom::AppStatePtr>());
  remote_->GetAppStates(std::move(wrapped_callback));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::RunPeriodicTasksCallback wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback));
  remote_->RunPeriodicTasks(std::move(wrapped_callback));
}

void UpdateServiceProxy::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::UpdateCallback state_change_observer_callback =
      MakeStateChangeObserver(
          base::BindPostTaskToCurrentDefault(state_update),
          base::BindPostTaskToCurrentDefault(std::move(callback)));
  remote_->CheckForUpdate(
      app_id, static_cast<mojom::UpdateService::Priority>(priority),
      static_cast<mojom::UpdateService::PolicySameVersionUpdate>(
          policy_same_version_update),
      std::move(state_change_observer_callback));
}

void UpdateServiceProxy::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::UpdateCallback state_change_observer_callback =
      MakeStateChangeObserver(
          base::BindPostTaskToCurrentDefault(state_update),
          base::BindPostTaskToCurrentDefault(std::move(callback)));
  remote_->Update(app_id, install_data_index,
                  static_cast<mojom::UpdateService::Priority>(priority),
                  static_cast<mojom::UpdateService::PolicySameVersionUpdate>(
                      policy_same_version_update),
                  /*do_update_check_only=*/false,
                  std::move(state_change_observer_callback));
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::UpdateAllCallback state_change_observer_callback =
      MakeStateChangeObserver(
          base::BindPostTaskToCurrentDefault(state_update),
          base::BindPostTaskToCurrentDefault(std::move(callback)));
  remote_->UpdateAll(std::move(state_change_observer_callback));
}

void UpdateServiceProxy::Install(const RegistrationRequest& registration,
                                 const std::string& client_install_data,
                                 const std::string& install_data_index,
                                 Priority priority,
                                 StateChangeCallback state_update,
                                 Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::InstallCallback state_change_observer_callback =
      MakeStateChangeObserver(
          base::BindPostTaskToCurrentDefault(state_update),
          base::BindPostTaskToCurrentDefault(std::move(callback)));
  remote_->Install(MakeRegistrationRequest(registration), client_install_data,
                   install_data_index,
                   static_cast<mojom::UpdateService::Priority>(priority),
                   std::move(state_change_observer_callback));
}

void UpdateServiceProxy::CancelInstalls(const std::string& app_id) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->CancelInstalls(app_id);
}

void UpdateServiceProxy::RunInstaller(const std::string& app_id,
                                      const base::FilePath& installer_path,
                                      const std::string& install_args,
                                      const std::string& install_data,
                                      const std::string& install_settings,
                                      StateChangeCallback state_update,
                                      Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  mojom::UpdateService::RunInstallerCallback state_change_observer_callback =
      MakeStateChangeObserver(
          base::BindPostTaskToCurrentDefault(state_update),
          base::BindPostTaskToCurrentDefault(std::move(callback)));
  remote_->RunInstaller(app_id, installer_path, install_args, install_data,
                        install_settings,
                        std::move(state_change_observer_callback));
}

void UpdateServiceProxy::OnConnected(
    mojo::PendingReceiver<mojom::UpdateService> pending_receiver,
    absl::optional<mojo::PlatformChannelEndpoint> endpoint) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connecting_ = false;
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
      &UpdateServiceProxy::OnDisconnected, weak_factory_.GetWeakPtr()));
}

void UpdateServiceProxy::OnDisconnected() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.reset();
  remote_.reset();
}

UpdateServiceProxy::~UpdateServiceProxy() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpdateServiceProxy::EnsureConnecting() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_ || connecting_) {
    return;
  }
  connecting_ = true;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &Connect, scope_, 0, base::Time::Now() + kConnectionTimeout,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &UpdateServiceProxy::OnConnected, weak_factory_.GetWeakPtr(),
              remote_.BindNewPipeAndPassReceiver()))));
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    const base::TimeDelta& timeout) {
  return base::MakeRefCounted<UpdateServiceProxy>(scope, timeout);
}

}  // namespace updater
