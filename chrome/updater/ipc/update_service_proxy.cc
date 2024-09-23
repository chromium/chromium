// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace updater {
namespace {

bool CanRetry(int try_count) {
  return try_count < 3;
}

template <typename T>
using DoneFunc =
    void (*)(scoped_refptr<UpdateServiceProxy>,
             base::RepeatingCallback<void(
                 base::OnceCallback<void(base::expected<T, RpcError>)>)> call,
             base::OnceCallback<void(T)>,
             T,
             int,
             base::expected<T, RpcError>);

template <typename T>
void CallDone(scoped_refptr<UpdateServiceProxy> proxy,
              base::RepeatingCallback<void(
                  base::OnceCallback<void(base::expected<T, RpcError>)>)> call,
              base::OnceCallback<void(T)> callback,
              T default_response,
              int try_count,
              base::expected<T, RpcError> result) {
  if (!result.has_value() && CanRetry(try_count)) {
    call.Run(base::BindOnce(static_cast<DoneFunc<T>>(&CallDone), proxy, call,
                            std::move(callback), default_response,
                            try_count + 1));
    return;
  }
  std::move(callback).Run(result.value_or(default_response));
}

}  // namespace

UpdateServiceProxy::UpdateServiceProxy(
    scoped_refptr<UpdateServiceProxyImpl> proxy)
    : proxy_(proxy) {}

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::GetVersion, proxy_);
  call.Run(base::BindOnce(
      static_cast<DoneFunc<base::Version>>(&CallDone),
      base::WrapRefCounted(this), call,
      base::BindOnce(
          [](base::OnceCallback<void(const base::Version&)> callback,
             base::Version version) { std::move(callback).Run(version); },
          std::move(callback)),
      base::Version(), 1));
}

void UpdateServiceProxy::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call =
      base::BindRepeating(&UpdateServiceProxyImpl::FetchPolicies, proxy_);
  call.Run(base::BindOnce(static_cast<DoneFunc<int>>(&CallDone),
                          base::WrapRefCounted(this), call, std::move(callback),
                          kErrorIpcDisconnect, 1));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::RegisterApp, proxy_,
                                  request);
  call.Run(base::BindOnce(static_cast<DoneFunc<int>>(&CallDone),
                          base::WrapRefCounted(this), call, std::move(callback),
                          kErrorIpcDisconnect, 1));
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call =
      base::BindRepeating(&UpdateServiceProxyImpl::GetAppStates, proxy_);
  call.Run(base::BindOnce(
      static_cast<DoneFunc<std::vector<AppState>>>(&CallDone),
      base::WrapRefCounted(this), call,
      base::BindOnce(
          [](base::OnceCallback<void(const std::vector<AppState>&)> callback,
             std::vector<AppState> value) { std::move(callback).Run(value); },
          std::move(callback)),
      std::vector<AppState>(), 1));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call =
      base::BindRepeating(&UpdateServiceProxyImpl::RunPeriodicTasks, proxy_);
  call.Run(base::BindOnce(
      static_cast<DoneFunc<int>>(&CallDone), base::WrapRefCounted(this), call,
      base::BindOnce([](base::OnceClosure callback,
                        int /*result*/) { std::move(callback).Run(); },
                     std::move(callback)),
      kErrorIpcDisconnect, 1));
}

void UpdateServiceProxy::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::CheckForUpdate,
                                  proxy_, app_id, priority,
                                  policy_same_version_update, state_update);
  call.Run(
      base::BindOnce(static_cast<DoneFunc<UpdateService::Result>>(&CallDone),
                     base::WrapRefCounted(this), call, std::move(callback),
                     UpdateService::Result::kIPCConnectionFailed, 1));
}

void UpdateServiceProxy::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::Update, proxy_,
                                  app_id, install_data_index, priority,
                                  policy_same_version_update, state_update);
  call.Run(
      base::BindOnce(static_cast<DoneFunc<UpdateService::Result>>(&CallDone),
                     base::WrapRefCounted(this), call, std::move(callback),
                     UpdateService::Result::kIPCConnectionFailed, 1));
}

void UpdateServiceProxy::UpdateAll(
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::UpdateAll, proxy_,
                                  state_update);
  call.Run(
      base::BindOnce(static_cast<DoneFunc<UpdateService::Result>>(&CallDone),
                     base::WrapRefCounted(this), call, std::move(callback),
                     UpdateService::Result::kIPCConnectionFailed, 1));
}

void UpdateServiceProxy::Install(
    const RegistrationRequest& registration,
    const std::string& client_install_data,
    const std::string& install_data_index,
    Priority priority,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::Install, proxy_,
                                  registration, client_install_data,
                                  install_data_index, priority, state_update);
  call.Run(
      base::BindOnce(static_cast<DoneFunc<UpdateService::Result>>(&CallDone),
                     base::WrapRefCounted(this), call, std::move(callback),
                     UpdateService::Result::kIPCConnectionFailed, 1));
}

void UpdateServiceProxy::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_->CancelInstalls(app_id);
}

void UpdateServiceProxy::RunInstaller(
    const std::string& app_id,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    const std::string& install_settings,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto call = base::BindRepeating(&UpdateServiceProxyImpl::RunInstaller, proxy_,
                                  app_id, installer_path, install_args,
                                  install_data, install_settings, state_update);
  call.Run(
      base::BindOnce(static_cast<DoneFunc<UpdateService::Result>>(&CallDone),
                     base::WrapRefCounted(this), call, std::move(callback),
                     UpdateService::Result::kIPCConnectionFailed, 1));
}

UpdateServiceProxy::~UpdateServiceProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
