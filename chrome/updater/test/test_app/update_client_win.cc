// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/update_client_win.h"

#include <atlsecurity.h>
#include <sddl.h>
#include <wrl/implements.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_bstr.h"
#include "base/win/win_util.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/update_service.h"

namespace updater {

namespace {

HRESULT CreateUpdaterInterface(Microsoft::WRL::ComPtr<IUpdater>* updater) {
  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_UpdaterClass, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to instantiate the update server. " << std::hex << hr;
    return hr;
  }

  hr = server.As(updater);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to query the updater interface. " << std::hex << hr;
    return hr;
  }

  return hr;
}

class UpdaterObserver
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterObserver(Microsoft::WRL::ComPtr<IUpdater> updater,
                  UpdateService::Callback callback);
  UpdaterObserver(const UpdaterObserver&) = delete;
  UpdaterObserver& operator=(const UpdaterObserver&) = delete;

  // Overrides for IUpdaterObserver.
  IFACEMETHODIMP OnStateChange(IUpdateState* update_state) override;
  IFACEMETHODIMP OnComplete(ICompleteStatus* status) override;

 private:
  ~UpdaterObserver() override;

  scoped_refptr<base::SequencedTaskRunner> observer_com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::Callback callback_;
};

UpdaterObserver::UpdaterObserver(Microsoft::WRL::ComPtr<IUpdater> updater,
                                 UpdateService::Callback callback)
    : observer_com_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      updater_(updater),
      callback_(std::move(callback)) {}

UpdaterObserver::~UpdaterObserver() = default;

HRESULT UpdaterObserver::OnStateChange(IUpdateState* update_state) {
  return E_NOTIMPL;
}

HRESULT UpdaterObserver::OnComplete(ICompleteStatus* status) {
  DCHECK(status);

  LONG code = 0;
  base::win::ScopedBstr message;

  if (FAILED(status->get_statusCode(&code))) {
    LOG(ERROR) << "No status code from ICompleteStatus.";
    observer_com_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  UpdateService::Result::kUpdateCheckFailed));
  }

  if (FAILED(status->get_statusMessage(message.Receive()))) {
    LOG(ERROR) << "No message from ICompleteStatus.";
    observer_com_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  UpdateService::Result::kUpdateCheckFailed));
  }

  VLOG(2) << "UpdaterObserverImpl::OnComplete(" << code << ", " << message.Get()
          << ")";

  observer_com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                static_cast<UpdateService::Result>(code)));
  updater_ = nullptr;
  return S_OK;
}

}  // namespace

UpdateClientWin::UpdateClientWin()
    : com_task_runner_(base::ThreadPool::CreateCOMSTATaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

UpdateClientWin::~UpdateClientWin() = default;

bool UpdateClientWin::CanDialIPC() {
  return true;
}

void UpdateClientWin::BeginRegister(const std::string& brand_code,
                                    const std::string& tag,
                                    const std::string& version,
                                    UpdateService::Callback callback) {
  // TODO(1068693): Implement TestApp Functionality.
  NOTIMPLEMENTED();
}

void UpdateClientWin::BeginUpdateCheck(
    UpdateService::StateChangeCallback state_change,
    UpdateService::Callback callback) {
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateClientWin::UpdateCheckInternal, this, state_change,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 UpdateService::Callback callback,
                 UpdateService::Result result) {
                task_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              task_runner(), std::move(callback))));
}

void UpdateClientWin::RegisterInternal(const std::string& brand_code,
                                       const std::string& tag,
                                       const std::string& version,
                                       UpdateService::Callback callback) {}

void UpdateClientWin::UpdateCheckInternal(
    UpdateService::StateChangeCallback state_change,
    UpdateService::Callback callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  if (!updater_) {
    HRESULT hr = CreateUpdaterInterface(&updater_);
    if (FAILED(hr)) {
      return;
    }
  }

  auto observer =
      Microsoft::WRL::Make<UpdaterObserver>(updater_, std::move(callback));
  HRESULT hr =
      updater_->Update(base::ASCIIToUTF16(kTestAppId).c_str(), observer.Get());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to call IUpdater::UpdateAll " << std::hex << hr;
    UpdateService::UpdateState state;
    state.state = UpdateService::UpdateState::State::kNoUpdate;
    task_runner()->PostTask(FROM_HERE, base::BindOnce(state_change, state));
    return;
  }
}

scoped_refptr<UpdateClient> UpdateClient::Create() {
  return base::MakeRefCounted<UpdateClientWin>();
}

}  // namespace updater
