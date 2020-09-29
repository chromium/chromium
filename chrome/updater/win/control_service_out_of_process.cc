// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/control_service_out_of_process.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/win/constants.h"

namespace updater {
namespace {

using ICompleteStatusPtr = ::Microsoft::WRL::ComPtr<ICompleteStatus>;

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// This class implements the IUpdaterObserver interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread. However, its
// functions are invoked directly by COM RPC, and they are not sequenced through
// the thread task runner. This means that sequence checkers can't be used in
// this class.
class UpdaterControlObserver
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterControlObserver(
      Microsoft::WRL::ComPtr<IUpdaterControl> updater_control,
      base::OnceClosure callback)
      : com_task_runner_(base::SequencedTaskRunnerHandle::Get()),
        updater_control_(updater_control),
        callback_(std::move(callback)) {}

  UpdaterControlObserver(const UpdaterControlObserver&) = delete;
  UpdaterControlObserver& operator=(const UpdaterControlObserver&) = delete;

  // Overrides for IUpdaterObserver.
  IFACEMETHODIMP OnStateChange(IUpdateState* update_state) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP OnComplete(ICompleteStatus* complete_status) override {
    DCHECK(complete_status);
    DVLOG(2) << __func__ << " returned " << QueryStatus(complete_status) << ".";
    com_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdaterControlObserver::OnCompleteOnSTA,
                                  base::WrapRefCounted(this)));
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceClosure Disconnect();

 private:
  ~UpdaterControlObserver() override = default;

  // Called in sequence on the |com_task_runner_|.
  void OnCompleteOnSTA();

  // Returns the value of the status code.]
  LONG QueryStatus(ICompleteStatus* complete_status);

  // Bound to the STA thread.
  THREAD_CHECKER(thread_checker_);

  // Bound to the STA thread.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdaterControl> updater_control_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  base::OnceClosure callback_;
};

base::OnceClosure UpdaterControlObserver::Disconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  updater_control_ = nullptr;
  return std::move(callback_);
}

LONG UpdaterControlObserver::QueryStatus(ICompleteStatus* complete_status) {
  DCHECK(complete_status);

  LONG code = 0;
  base::win::ScopedBstr message;
  CHECK(SUCCEEDED(complete_status->get_statusCode(&code)));

  return code;
}

void UpdaterControlObserver::OnCompleteOnSTA() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  updater_control_ = nullptr;

  if (!callback_) {
    DVLOG(2) << "Skipping posting the completion callback.";
    return;
  }
  com_task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback_)));
}

}  // namespace

ControlServiceOutOfProcess::ControlServiceOutOfProcess(ServiceScope /*scope*/)
    : com_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits)) {}

ControlServiceOutOfProcess::~ControlServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlServiceOutOfProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlServiceOutOfProcess::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reposts the call to the COM task runner. Adapts |callback| so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ControlServiceOutOfProcess::RunOnSTA, this,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 base::OnceClosure callback) {
                taskrunner->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback)));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ControlServiceOutOfProcess::RunOnSTA(base::OnceClosure callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterControlServiceClass, nullptr,
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

  // The COM RPC takes ownership of the |observer| and owns a reference to
  // the updater object as well. As long as the |observer| retains this
  // reference to the updater control object, then the object is going to stay
  // alive.
  // The |observer| can drop its reference to the updater control object after
  // handling the last server callback, then the object model is torn down, and
  // finally, the execution flow returns back into the App object once the
  // completion callback is posted.
  auto observer = Microsoft::WRL::Make<UpdaterControlObserver>(
      updater_control, std::move(callback));
  hr = updater_control->Run(observer.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdaterControl::Run" << std::hex << hr;

    // Since the RPC call returned an error, it can't be determined what the
    // state of the update server is. The observer may or may not post any
    // callback. Disconnecting the observer resolves this ambiguity and
    // transfers the ownership of the callback back to the owner of the
    // observer.
    observer->Disconnect().Run();
    return;
  }
}

void ControlServiceOutOfProcess::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ControlServiceOutOfProcess::InitializeUpdateServiceOnSTA, this,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 base::OnceClosure callback) {
                taskrunner->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback)));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ControlServiceOutOfProcess::InitializeUpdateServiceOnSTA(
    base::OnceClosure callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterControlServiceClass, nullptr,
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

  auto observer = Microsoft::WRL::Make<UpdaterControlObserver>(
      updater_control, std::move(callback));
  hr = updater_control->InitializeUpdateService(observer.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdaterControl::InitializeUpdateService"
             << std::hex << hr;
    observer->Disconnect().Run();
    return;
  }
}
}  // namespace updater
