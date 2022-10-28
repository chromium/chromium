// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_linux.h"
#include "chrome/updater/service_proxy_factory.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/linux/mojom/updater_service.mojom.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace updater {

// TODO(crbug.com/1276169) - implement.
class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl> {
 public:
  explicit UpdateServiceProxyImpl(UpdaterScope /*scope*/,
                                  mojo::Remote<mojom::UpdateService> remote)
      : remote_(std::move(remote)) {}

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
    NOTIMPLEMENTED();
  }

  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) {
    NOTIMPLEMENTED();
  }

  void GetAppStates(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
          callback) {
    NOTIMPLEMENTED();
  }

  void RunPeriodicTasks(base::OnceClosure callback) { NOTIMPLEMENTED(); }

  void UpdateAll(UpdateService::StateChangeCallback state_update,
                 UpdateService::Callback callback) {
    NOTIMPLEMENTED();
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              UpdateService::StateChangeCallback state_update,
              UpdateService::Callback callback) {
    NOTIMPLEMENTED();
  }

  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               UpdateService::Priority priority,
               UpdateService::StateChangeCallback state_update,
               UpdateService::Callback callback) {
    NOTIMPLEMENTED();
  }

  void CancelInstalls(const std::string& app_id) { NOTIMPLEMENTED(); }

  void RunInstaller(const std::string& app_id,
                    const base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    UpdateService::StateChangeCallback state_update,
                    UpdateService::Callback callback) {
    NOTIMPLEMENTED();
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImpl>;
  virtual ~UpdateServiceProxyImpl() = default;

  mojo::Remote<mojom::UpdateService> remote_;
};

UpdateServiceProxy::UpdateServiceProxy(
    UpdaterScope updater_scope,
    mojo::Remote<mojom::UpdateService> remote)
    : impl_(base::MakeRefCounted<UpdateServiceProxyImpl>(updater_scope,
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
    const base::TimeDelta& /*get_version_timeout*/) {
  // TODO(crbug.com/1375890): This code assumes that the out-of-process service
  // is running. The creation and lifetime management of the server needs to be
  // handled.
  mojo::NamedPlatformChannel::Options options;
  options.server_name = kUpdateServerChannelName;
  mojo::NamedPlatformChannel named_channel(options);

  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kUpdateServerChannelPipeName);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 named_channel.TakeServerEndpoint());
  mojo::Remote<mojom::UpdateService> remote({std::move(pipe), 0});
  remote.set_disconnect_handler(base::BindOnce([]() {
    LOG(ERROR) << "UpdateService remote has unexpectedly disconnected.";
  }));
  return CreateUpdateServiceProxy(scope, std::move(remote));
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope scope,
    mojo::Remote<mojom::UpdateService> remote) {
  return base::MakeRefCounted<UpdateServiceProxy>(scope, std::move(remote));
}

}  // namespace updater
