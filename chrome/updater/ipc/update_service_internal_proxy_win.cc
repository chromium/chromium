// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy_win.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <ios>
#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/threading/platform_thread.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/ipc/proxy_impl_base_win.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

// This class implements the IUpdaterInternalCallback interface and exposes it
// as a COM object. The class has thread-affinity for the STA thread.
class UpdaterInternalCallback
    : public DynamicIIDsImpl<IUpdaterInternalCallback,
                             __uuidof(IUpdaterInternalCallbackUser),
                             __uuidof(IUpdaterInternalCallbackSystem)> {
 public:
  explicit UpdaterInternalCallback(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  UpdaterInternalCallback(const UpdaterInternalCallback&) = delete;
  UpdaterInternalCallback& operator=(const UpdaterInternalCallback&) = delete;

  // Overrides for IUpdaterInternalCallback.
  //
  // Invoked by COM RPC on the apartment thread (STA) when the call to any of
  // the non-blocking `UpdateServiceInternalProxy` functions completes.
  IFACEMETHODIMP Run(LONG result) override;

  // Disconnects this callback from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceClosure Disconnect();

 private:
  ~UpdaterInternalCallback() override {
    CHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run();
  }

  // The reference of the thread this object is bound to.
  base::PlatformThreadRef com_thread_ref_;

  // Called by IUpdaterInternalCallback::Run when the COM RPC call is done.
  base::OnceClosure callback_;
};

IFACEMETHODIMP UpdaterInternalCallback::Run(LONG result) {
  CHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
  VLOG(2) << __func__ << " result " << result << ".";
  return S_OK;
}

base::OnceClosure UpdaterInternalCallback::Disconnect() {
  CHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
  VLOG(2) << __func__;
  return std::move(callback_);
}

}  // namespace

class UpdateServiceInternalProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl>,
      public ProxyImplBase<UpdateServiceInternalProxyImpl,
                           IUpdaterInternal,
                           __uuidof(IUpdaterInternalUser),
                           __uuidof(IUpdaterInternalSystem)> {
 public:
  explicit UpdateServiceInternalProxyImpl(UpdaterScope scope)
      : ProxyImplBase(scope) {}

  static auto GetClassGuid(UpdaterScope scope) {
    return IsSystemInstall(scope) ? __uuidof(UpdaterInternalSystemClass)
                                  : __uuidof(UpdaterInternalUserClass);
  }

  void Run(base::OnceClosure callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceInternalProxyImpl::RunOnSTA, this,
                               std::move(callback)));
  }

  void Hello(base::OnceClosure callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceInternalProxyImpl::HelloOnSTA,
                               this, std::move(callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl>;
  ~UpdateServiceInternalProxyImpl() = default;

  void RunOnSTA(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run();
      return;
    }
    auto callback_wrapper =
        Microsoft::WRL::Make<UpdaterInternalCallback>(std::move(callback));
    HRESULT hr = get_interface()->Run(callback_wrapper.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdaterInternal::Run" << std::hex << hr;
      callback_wrapper->Disconnect().Run();
      return;
    }
  }

  void HelloOnSTA(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run();
      return;
    }
    auto callback_wrapper =
        Microsoft::WRL::Make<UpdaterInternalCallback>(std::move(callback));
    HRESULT hr = get_interface()->Hello(callback_wrapper.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdaterInternal::Hello" << std::hex << hr;
      callback_wrapper->Disconnect().Run();
      return;
    }
  }
};

UpdateServiceInternalProxy::UpdateServiceInternalProxy(UpdaterScope scope)
    : impl_(base::MakeRefCounted<UpdateServiceInternalProxyImpl>(scope)) {}

UpdateServiceInternalProxy::~UpdateServiceInternalProxy() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateServiceInternalProxyImpl::Destroy(impl_);
  CHECK_EQ(impl_, nullptr);
}

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Run(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceInternalProxy::Hello(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Hello(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope updater_scope) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(updater_scope);
}

}  // namespace updater
