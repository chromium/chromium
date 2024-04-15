// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy_win.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <ios>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/ipc/proxy_impl_base_win.h"
#include "chrome/updater/ipc/update_service_internal_proxy.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

class UpdaterInternalCallback
    : public DYNAMICIIDSIMPL(IUpdaterInternalCallback) {
 public:
  explicit UpdaterInternalCallback(
      base::OnceCallback<void(std::optional<RpcError>)> callback)
      : callback_(std::move(callback)) {}
  UpdaterInternalCallback(const UpdaterInternalCallback&) = delete;
  UpdaterInternalCallback& operator=(const UpdaterInternalCallback&) = delete;

  // Overrides for IUpdaterInternalCallback. Called on a system thread by COM
  // RPC.
  IFACEMETHODIMP Run(LONG result) override;

  // Disconnects this callback from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(std::optional<RpcError>)> Disconnect();

 private:
  ~UpdaterInternalCallback() override {
    if (callback_) {
      std::move(callback_).Run(std::nullopt);
    }
  }

  // Called by IUpdaterInternalCallback::Run when the COM RPC call is done.
  base::OnceCallback<void(std::optional<RpcError>)> callback_;
};

IFACEMETHODIMP UpdaterInternalCallback::Run(LONG result) {
  VLOG(2) << __func__ << " result " << result << ".";
  return S_OK;
}

base::OnceCallback<void(std::optional<RpcError>)>
UpdaterInternalCallback::Disconnect() {
  VLOG(2) << __func__;
  return std::move(callback_);
}

}  // namespace

class UpdateServiceInternalProxyImplImpl
    : public base::RefCountedThreadSafe<UpdateServiceInternalProxyImplImpl>,
      public ProxyImplBase<UpdateServiceInternalProxyImplImpl,
                           IUpdaterInternal,
                           __uuidof(IUpdaterInternalUser),
                           __uuidof(IUpdaterInternalSystem)> {
 public:
  explicit UpdateServiceInternalProxyImplImpl(UpdaterScope scope)
      : ProxyImplBase(scope) {}

  static auto GetClassGuid(UpdaterScope scope) {
    return IsSystemInstall(scope) ? __uuidof(UpdaterInternalSystemClass)
                                  : __uuidof(UpdaterInternalUserClass);
  }

  void Run(base::OnceCallback<void(std::optional<RpcError>)> callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceInternalProxyImplImpl::RunOnTaskRunner,
                       this, std::move(callback)));
  }

  void Hello(base::OnceCallback<void(std::optional<RpcError>)> callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceInternalProxyImplImpl::HelloOnTaskRunner,
                       this, std::move(callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceInternalProxyImplImpl>;
  ~UpdateServiceInternalProxyImplImpl() = default;

  void RunOnTaskRunner(
      base::OnceCallback<void(std::optional<RpcError>)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT connection = ConnectToServer(); FAILED(connection)) {
      std::move(callback).Run(connection);
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterInternalCallback>(std::move(callback));
    HRESULT hr = get_interface()->Run(callback_wrapper.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdaterInternal::Run" << std::hex << hr;
      callback_wrapper->Disconnect().Run(hr);
      return;
    }
  }

  void HelloOnTaskRunner(
      base::OnceCallback<void(std::optional<RpcError>)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT connection = ConnectToServer(); FAILED(connection)) {
      std::move(callback).Run(connection);
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterInternalCallback>(std::move(callback));
    HRESULT hr = get_interface()->Hello(callback_wrapper.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdaterInternal::Hello" << std::hex << hr;
      callback_wrapper->Disconnect().Run(hr);
      return;
    }
  }
};

UpdateServiceInternalProxyImpl::UpdateServiceInternalProxyImpl(
    UpdaterScope scope)
    : impl_(base::MakeRefCounted<UpdateServiceInternalProxyImplImpl>(scope)) {}

UpdateServiceInternalProxyImpl::~UpdateServiceInternalProxyImpl() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateServiceInternalProxyImplImpl::Destroy(std::move(impl_));
}

void UpdateServiceInternalProxyImpl::Run(
    base::OnceCallback<void(std::optional<RpcError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Run(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceInternalProxyImpl::Hello(
    base::OnceCallback<void(std::optional<RpcError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Hello(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope updater_scope) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(
      base::MakeRefCounted<UpdateServiceInternalProxyImpl>(updater_scope));
}

}  // namespace updater
