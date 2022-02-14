// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/update_service_internal_proxy.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/wrl_module_initializer.h"

namespace updater {
namespace {

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// This class implements the IUpdaterInternalCallback interface and exposes it
// as a COM object. The class has thread-affinity for the STA thread.
class UpdaterInternalCallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterInternalCallback> {
 public:
  UpdaterInternalCallback(
      Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal,
      base::OnceClosure callback)
      : updater_internal_(updater_internal), callback_(std::move(callback)) {}

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
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run();
  }

  // The reference of the thread this object is bound to.
  base::PlatformThreadRef com_thread_ref_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal_;

  // Called by IUpdaterInternalCallback::Run when the COM RPC call is done.
  base::OnceClosure callback_;
};

IFACEMETHODIMP UpdaterInternalCallback::Run(LONG result) {
  DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
  DVLOG(2) << __func__ << " result " << result << ".";
  return S_OK;
}

base::OnceClosure UpdaterInternalCallback::Disconnect() {
  DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
  DVLOG(2) << __func__;
  updater_internal_ = nullptr;
  return std::move(callback_);
}

}  // namespace

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope updater_scope) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(updater_scope);
}

UpdateServiceInternalProxy::UpdateServiceInternalProxy(UpdaterScope scope)
    : scope_(scope),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      com_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits)) {
  WRLModuleInitializer::Get();
}

UpdateServiceInternalProxy::~UpdateServiceInternalProxy() = default;

void UpdateServiceInternalProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);
}

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);

  com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdateServiceInternalProxy::RunOnSTA, this,
                                base::BindPostTask(main_task_runner_,
                                                   std::move(callback))));
}

CLSID UpdateServiceInternalProxy::GetInternalClass() const {
  switch (scope_) {
    case UpdaterScope::kUser:
      return __uuidof(UpdaterInternalUserClass);
    case UpdaterScope::kSystem:
      return __uuidof(UpdaterInternalSystemClass);
  }
}

void UpdateServiceInternalProxy::RunOnSTA(base::OnceClosure callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  ::Sleep(kCreateUpdaterInstanceDelayMs);
  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(GetInternalClass(), nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to instantiate the updater internal server. "
             << std::hex << hr;
    std::move(callback).Run();
    return;
  }

  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
  hr = server.As(&updater_internal);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to query the updater_internal interface. " << std::hex
             << hr;
    std::move(callback).Run();
    return;
  }

  // The COM RPC takes ownership of the `rpc_callback` and owns a reference to
  // the `updater_internal` object as well. As long as the `rpc_callback`
  // retains this reference to the `updater_internal` object, then the object
  // is going to stay alive. Once the server has notified, then released its
  // last reference to the `rpc_callback` object, the `rpc_callback` is
  // destroyed, and as a result, the last reference to `updater_internal` is
  // released as well, which causes the destruction of the `updater_internal`
  // object.
  auto rpc_callback = Microsoft::WRL::Make<UpdaterInternalCallback>(
      updater_internal, std::move(callback));
  hr = updater_internal->Run(rpc_callback.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdaterInternal::Run" << std::hex << hr;

    // Since the RPC call returned an error, it can't be determined what the
    // state of the update server is. The RPC callback may or may not have run.
    // Disconnecting the object resolves this ambiguity and transfers the
    // ownership of the callback back to the caller.
    rpc_callback->Disconnect().Run();
    return;
  }
}

void UpdateServiceInternalProxy::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);

  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateServiceInternalProxy::InitializeUpdateServiceOnSTA, this,
          base::BindPostTask(main_task_runner_, std::move(callback))));
}

void UpdateServiceInternalProxy::InitializeUpdateServiceOnSTA(
    base::OnceClosure callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  ::Sleep(kCreateUpdaterInstanceDelayMs);
  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(GetInternalClass(), nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to instantiate the updater internal server. "
             << std::hex << hr;
    std::move(callback).Run();
    return;
  }

  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
  hr = server.As(&updater_internal);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to query the updater_internal interface. " << std::hex
             << hr;
    std::move(callback).Run();
    return;
  }

  auto rpc_callback = Microsoft::WRL::Make<UpdaterInternalCallback>(
      updater_internal, std::move(callback));
  hr = updater_internal->InitializeUpdateService(rpc_callback.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdaterInternal::InitializeUpdateService"
             << std::hex << hr;
    rpc_callback->Disconnect().Run();
    return;
  }
}

}  // namespace updater
