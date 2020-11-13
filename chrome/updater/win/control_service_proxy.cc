// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/control_service_proxy.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/win/constants.h"

namespace updater {
namespace {

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// This class implements the IUpdaterControlCallback interface and exposes it as
// a COM object. The class has thread-affinity for the STA thread.
class UpdaterControlCallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterControlCallback> {
 public:
  UpdaterControlCallback(
      Microsoft::WRL::ComPtr<IUpdaterControl> updater_control,
      base::OnceClosure callback)
      : updater_control_(updater_control), callback_(std::move(callback)) {}

  UpdaterControlCallback(const UpdaterControlCallback&) = delete;
  UpdaterControlCallback& operator=(const UpdaterControlCallback&) = delete;

  // Overrides for IUpdaterControlCallback.
  //
  // Invoked by COM RPC on the apartment thread (STA) when the call to any of
  // the non-blocking `ControlServiceProxy` functions completes.
  IFACEMETHODIMP Run(LONG result) override;

  // Disconnects this callback from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceClosure Disconnect();

 private:
  ~UpdaterControlCallback() override = default;

  void RunOnSTA();

  // Bound to the STA thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Sequences COM function calls.
  scoped_refptr<base::SingleThreadTaskRunner> STA_task_runner_ =
      base::ThreadTaskRunnerHandle::Get();

  // The thread id of the STA thread.
  const base::PlatformThreadId STA_thread_id_ =
      base::PlatformThread::CurrentId();

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdaterControl> updater_control_;

  // Called by IUpdaterControlCallback::Run when the COM RPC call is done.
  base::OnceClosure callback_;
};

IFACEMETHODIMP UpdaterControlCallback::Run(LONG result) {
  DVLOG(2) << __func__ << " result " << result << ".";

  // Since this function is invoked directly by COM RPC, the code can only
  // assert its OS thread-affinity but not its task runner sequence-affinity.
  // For this reason, the implementation is delegated to a helper function,
  // which is sequenced by `STA_task_runner`.
  DCHECK_EQ(base::PlatformThread::CurrentId(), STA_thread_id_);
  STA_task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&UpdaterControlCallback::RunOnSTA,
                                            base::WrapRefCounted(this)));
  return S_OK;
}

base::OnceClosure UpdaterControlCallback::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;
  updater_control_ = nullptr;
  return std::move(callback_);
}

void UpdaterControlCallback::RunOnSTA() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updater_control_ = nullptr;

  if (!callback_) {
    DVLOG(2) << "Skipping posting the completion callback.";
    return;
  }
  STA_task_runner_->PostTask(FROM_HERE, std::move(callback_));
}

}  // namespace

ControlServiceProxy::ControlServiceProxy(ServiceScope /*scope*/)
    : STA_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits)) {}

ControlServiceProxy::~ControlServiceProxy() = default;

void ControlServiceProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlServiceProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  STA_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ControlServiceProxy::RunOnSTA, this,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::OnceClosure callback) {
                task_runner->PostTask(FROM_HERE, std::move(callback));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ControlServiceProxy::RunOnSTA(base::OnceClosure callback) {
  DCHECK(STA_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterControlClass, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to instantiate the updater control server. " << std::hex
             << hr;
    std::move(callback).Run();
    return;
  }

  Microsoft::WRL::ComPtr<IUpdaterControl> updater_control;
  hr = server.As(&updater_control);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to query the updater_control interface. " << std::hex
             << hr;
    std::move(callback).Run();
    return;
  }

  // The `rpc_callback` takes ownership of the `callback` and owns a reference
  // to the updater object as well. As long as the `rpc_callback` retains this
  // reference to the updater control object, then the object is going to stay
  // alive.
  // The `rpc_callback` drops its reference to the updater control object when
  // handling the last server callback. After that, the object model is torn
  // down, and the execution flow returns back into the App object when
  // `callback` is posted.
  auto rpc_callback = Microsoft::WRL::Make<UpdaterControlCallback>(
      updater_control, std::move(callback));
  hr = updater_control->Run(rpc_callback.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdaterControl::Run" << std::hex << hr;

    // Since the RPC call returned an error, it can't be determined what the
    // state of the update server is. The RPC callback may or may not have run.
    // Disconnecting the object resolves this ambiguity and transfers the
    // ownership of the callback back to the caller.
    rpc_callback->Disconnect().Run();
    return;
  }
}

void ControlServiceProxy::InitializeUpdateService(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  STA_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ControlServiceProxy::InitializeUpdateServiceOnSTA, this,
          base::BindOnce(
              [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 base::OnceClosure callback) {
                task_runner->PostTask(FROM_HERE,
                                      base::BindOnce(std::move(callback)));
              },
              STA_task_runner_, std::move(callback))));
}

void ControlServiceProxy::InitializeUpdateServiceOnSTA(
    base::OnceClosure callback) {
  DCHECK(STA_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterControlClass, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to instantiate the updater control server. " << std::hex
             << hr;
    std::move(callback).Run();
    return;
  }

  Microsoft::WRL::ComPtr<IUpdaterControl> updater_control;
  hr = server.As(&updater_control);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to query the updater_control interface. " << std::hex
             << hr;
    std::move(callback).Run();
    return;
  }

  auto rpc_callback = Microsoft::WRL::Make<UpdaterControlCallback>(
      updater_control, std::move(callback));
  hr = updater_control->InitializeUpdateService(rpc_callback.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdaterControl::InitializeUpdateService"
             << std::hex << hr;
    rpc_callback->Disconnect().Run();
    return;
  }
}

}  // namespace updater
