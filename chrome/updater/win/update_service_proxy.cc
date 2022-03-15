// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/update_service_proxy.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/wrl_module_initializer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

using IUpdateStatePtr = ::Microsoft::WRL::ComPtr<IUpdateState>;
using ICompleteStatusPtr = ::Microsoft::WRL::ComPtr<ICompleteStatus>;

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// Creates an instance of COM server in the COM STA apartment.
HRESULT CreateServer(UpdaterScope scope,
                     Microsoft::WRL::ComPtr<IUnknown>& server) {
  ::Sleep(kCreateUpdaterInstanceDelayMs);
  HRESULT hr = ::CoCreateInstance(
      scope == UpdaterScope::kSystem ? __uuidof(UpdaterSystemClass)
                                     : __uuidof(UpdaterUserClass),
      nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  DVLOG_IF(2, FAILED(hr)) << "Failed to instantiate the update server: "
                          << std::hex << hr;
  return hr;
}

// Creates an instance of IUpdater in the COM STA apartment.
HRESULT CreateUpdater(UpdaterScope scope,
                      Microsoft::WRL::ComPtr<IUpdater>& updater) {
  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = CreateServer(scope, server);
  if (FAILED(hr))
    return hr;
  hr = server.As(&updater);
  DVLOG_IF(2, FAILED(hr)) << "Failed to query the updater interface: "
                          << std::hex << hr;
  return hr;
}

// This class implements the IUpdaterObserver interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread.
class UpdaterObserver
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterObserver(Microsoft::WRL::ComPtr<IUpdater> updater,
                  UpdateService::StateChangeCallback state_update_callback,
                  UpdateService::Callback callback)
      : updater_(updater),
        state_update_callback_(state_update_callback),
        callback_(std::move(callback)) {}
  UpdaterObserver(const UpdaterObserver&) = delete;
  UpdaterObserver& operator=(const UpdaterObserver&) = delete;

  // Overrides for IUpdaterObserver. These functions are called on the STA
  // thread directly by the COM RPC runtime.
  IFACEMETHODIMP OnStateChange(IUpdateState* update_state) override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DCHECK(update_state);

    if (!state_update_callback_) {
      DVLOG(2) << "Skipping posting the update state callback.";
      return S_OK;
    }

    state_update_callback_.Run(QueryUpdateState(update_state));
    return S_OK;
  }

  IFACEMETHODIMP OnComplete(ICompleteStatus* complete_status) override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DCHECK(complete_status);
    result_ = QueryResult(complete_status);
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  UpdateService::Callback Disconnect() {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DVLOG(2) << __func__;
    updater_ = nullptr;
    state_update_callback_.Reset();
    return std::move(callback_);
  }

 private:
  ~UpdaterObserver() override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run(result_);
  }

  UpdateService::UpdateState QueryUpdateState(IUpdateState* update_state) {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DCHECK(update_state);

    UpdateService::UpdateState update_service_state;
    {
      LONG val_state = 0;
      HRESULT hr = update_state->get_state(&val_state);
      if (SUCCEEDED(hr)) {
        using State = UpdateService::UpdateState::State;
        absl::optional<State> state = CheckedCastToEnum<State>(val_state);
        if (state)
          update_service_state.state = *state;
      }
    }
    {
      base::win::ScopedBstr app_id;
      HRESULT hr = update_state->get_appId(app_id.Receive());
      if (SUCCEEDED(hr))
        update_service_state.app_id = base::WideToUTF8(app_id.Get());
    }
    {
      base::win::ScopedBstr next_version;
      HRESULT hr = update_state->get_nextVersion(next_version.Receive());
      if (SUCCEEDED(hr)) {
        update_service_state.next_version =
            base::Version(base::WideToUTF8(next_version.Get()));
      }
    }
    {
      LONGLONG downloaded_bytes = -1;
      HRESULT hr = update_state->get_downloadedBytes(&downloaded_bytes);
      if (SUCCEEDED(hr))
        update_service_state.downloaded_bytes = downloaded_bytes;
    }
    {
      LONGLONG total_bytes = -1;
      HRESULT hr = update_state->get_totalBytes(&total_bytes);
      if (SUCCEEDED(hr))
        update_service_state.total_bytes = total_bytes;
    }
    {
      LONG install_progress = -1;
      HRESULT hr = update_state->get_installProgress(&install_progress);
      if (SUCCEEDED(hr))
        update_service_state.install_progress = install_progress;
    }
    {
      LONG val_error_category = 0;
      HRESULT hr = update_state->get_errorCategory(&val_error_category);
      if (SUCCEEDED(hr)) {
        using ErrorCategory = UpdateService::ErrorCategory;
        absl::optional<ErrorCategory> error_category =
            CheckedCastToEnum<ErrorCategory>(val_error_category);
        if (error_category)
          update_service_state.error_category = *error_category;
      }
    }
    {
      LONG error_code = -1;
      HRESULT hr = update_state->get_errorCode(&error_code);
      if (SUCCEEDED(hr))
        update_service_state.error_code = error_code;
    }
    {
      LONG extra_code1 = -1;
      HRESULT hr = update_state->get_extraCode1(&extra_code1);
      if (SUCCEEDED(hr))
        update_service_state.extra_code1 = extra_code1;
    }

    DVLOG(4) << update_service_state;
    return update_service_state;
  }

  UpdateService::Result QueryResult(ICompleteStatus* complete_status) {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DCHECK(complete_status);

    LONG code = 0;
    base::win::ScopedBstr message;
    CHECK(SUCCEEDED(complete_status->get_statusCode(&code)));

    DVLOG(2) << "ICompleteStatus::OnComplete(" << code << ")";
    return static_cast<UpdateService::Result>(code);
  }

  // The reference of the thread this object is bound to.
  base::PlatformThreadRef com_thread_ref_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  // Called by IUpdaterObserver::OnStateChange when update state changes occur.
  UpdateService::StateChangeCallback state_update_callback_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::Callback callback_;

  UpdateService::Result result_ = UpdateService::Result::kSuccess;
};

// This class implements the IUpdaterRegisterAppCallback interface and exposes
// it as a COM object. The class has thread-affinity for the STA thread.
class UpdaterRegisterAppCallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterRegisterAppCallback> {
 public:
  UpdaterRegisterAppCallback(Microsoft::WRL::ComPtr<IUpdater> updater,
                             UpdateService::RegisterAppCallback callback)
      : updater_(updater), callback_(std::move(callback)) {}
  UpdaterRegisterAppCallback(const UpdaterRegisterAppCallback&) = delete;
  UpdaterRegisterAppCallback& operator=(const UpdaterRegisterAppCallback&) =
      delete;

  // Overrides for IUpdaterRegisterAppCallback. This function is called on
  // the STA thread directly by the COM RPC runtime.
  IFACEMETHODIMP Run(LONG status_code) override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DVLOG(2) << __func__;
    status_code_ = status_code;
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  UpdateService::RegisterAppCallback Disconnect() {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DVLOG(2) << __func__;
    updater_ = nullptr;
    return std::move(callback_);
  }

 private:
  ~UpdaterRegisterAppCallback() override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run(RegistrationResponse(status_code_));
  }

  // The reference of the thread this object is bound to.
  base::PlatformThreadRef com_thread_ref_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::RegisterAppCallback callback_;

  LONG status_code_ = 0;
};

// This class implements the IUpdaterCallback interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread.
class UpdaterCallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterCallback> {
 public:
  UpdaterCallback(Microsoft::WRL::ComPtr<IUpdater> updater,
                  base::OnceCallback<void(LONG)> callback)
      : updater_(updater), callback_(std::move(callback)) {}
  UpdaterCallback(const UpdaterCallback&) = delete;
  UpdaterCallback& operator=(const UpdaterCallback&) = delete;

  // Overrides for IUpdaterCallback. This function is called on the STA
  // thread directly by the COM RPC runtime, and must be sequenced through
  // the task runner.
  IFACEMETHODIMP Run(LONG status_code) override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DVLOG(2) << __func__;
    status_code_ = status_code;
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(LONG)> Disconnect() {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    DVLOG(2) << __func__;
    updater_ = nullptr;
    return std::move(callback_);
  }

 private:
  ~UpdaterCallback() override {
    DCHECK_EQ(base::PlatformThreadRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run(status_code_);
  }

  // The reference of the thread this object is bound to.
  base::PlatformThreadRef com_thread_ref_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  base::OnceCallback<void(LONG)> callback_;

  LONG status_code_ = 0;
};

}  // namespace

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope) {
  return base::MakeRefCounted<UpdateServiceProxy>(updater_scope);
}

UpdateServiceProxy::UpdateServiceProxy(UpdaterScope updater_scope)
    : scope_(updater_scope),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      com_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits)) {
  WRLModuleInitializer::Get();
}

UpdateServiceProxy::~UpdateServiceProxy() = default;

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::InitializeSTA, this)
          .Then(base::BindOnce(
              &UpdateServiceProxy::GetVersionOnSTA, this,
              base::BindPostTask(main_task_runner_, std::move(callback)))));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     RegisterAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::InitializeSTA, this)
          .Then(base::BindOnce(
              &UpdateServiceProxy::RegisterAppOnSTA, this, request,
              base::BindPostTask(main_task_runner_, std::move(callback)))));
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::InitializeSTA, this)
          .Then(base::BindOnce(
              &UpdateServiceProxy::GetAppStatesSTA, this,
              base::BindPostTask(main_task_runner_, std::move(callback)))));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::InitializeSTA, this)
          .Then(base::BindOnce(
              &UpdateServiceProxy::RunPeriodicTasksOnSTA, this,
              base::BindPostTask(main_task_runner_, std::move(callback)))));
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);

  // Reposts the call to the COM task runner. Adapts `callback` so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::InitializeSTA, this)
          .Then(base::BindOnce(
              &UpdateServiceProxy::UpdateAllOnSTA, this,
              base::BindPostTask(main_task_runner_, state_update),
              base::BindPostTask(main_task_runner_, std::move(callback)))));
}

void UpdateServiceProxy::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority /*priority*/,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);

  // Reposts the call to the COM task runner. Adapts `callback` so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::InitializeSTA, this)
          .Then(base::BindOnce(
              &UpdateServiceProxy::UpdateOnSTA, this, app_id,
              install_data_index, policy_same_version_update,
              base::BindPostTask(main_task_runner_, state_update),
              base::BindPostTask(main_task_runner_, std::move(callback)))));
}

void UpdateServiceProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_main_);
  com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdateServiceProxy::UninitializeOnSTA, this));
}

HRESULT UpdateServiceProxy::InitializeSTA() {
  DCHECK(com_task_runner_->BelongsToCurrentThread());
  if (server_)
    return S_OK;
  return CreateServer(scope_, server_);
}

void UpdateServiceProxy::UninitializeOnSTA() {
  DCHECK(com_task_runner_->BelongsToCurrentThread());
  server_ = nullptr;
}

void UpdateServiceProxy::GetVersionOnSTA(
    base::OnceCallback<void(const base::Version&)> callback,
    HRESULT prev_hr) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  if (FAILED(prev_hr)) {
    std::move(callback).Run(base::Version());
    return;
  }
  Microsoft::WRL::ComPtr<IUpdater> updater;
  if (HRESULT hr = CreateUpdater(scope_, updater); FAILED(hr)) {
    std::move(callback).Run(base::Version());
    return;
  }
  base::win::ScopedBstr version;
  if (HRESULT hr = updater->GetVersion(version.Receive()); FAILED(hr)) {
    DVLOG(2) << "IUpdater::GetVersion failed: " << std::hex << hr;
    std::move(callback).Run(base::Version());
    return;
  }

  std::move(callback).Run(base::Version(base::WideToUTF8(version.Get())));
}

void UpdateServiceProxy::RegisterAppOnSTA(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback,
    HRESULT prev_hr) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  if (FAILED(prev_hr)) {
    std::move(callback).Run(RegistrationResponse(prev_hr));
    return;
  }
  Microsoft::WRL::ComPtr<IUpdater> updater;
  if (HRESULT hr = CreateUpdater(scope_, updater); FAILED(hr)) {
    std::move(callback).Run(RegistrationResponse(hr));
    return;
  }

  std::wstring app_id;
  std::wstring brand_code;
  std::wstring ap;
  std::wstring version;
  std::wstring existence_checker_path;
  if (![&]() {
        if (!base::UTF8ToWide(request.app_id.c_str(), request.app_id.size(),
                              &app_id)) {
          return false;
        }
        if (!base::UTF8ToWide(request.brand_code.c_str(),
                              request.brand_code.size(), &brand_code)) {
          return false;
        }
        if (!base::UTF8ToWide(request.ap.c_str(), request.ap.size(), &ap)) {
          return false;
        }
        std::string version_str = request.version.GetString();
        if (!base::UTF8ToWide(version_str.c_str(), version_str.size(),
                              &version)) {
          return false;
        }
        existence_checker_path = request.existence_checker_path.value();
        return true;
      }()) {
    std::move(callback).Run(RegistrationResponse(E_INVALIDARG));
    return;
  }

  auto callback_wrapper = Microsoft::WRL::Make<UpdaterRegisterAppCallback>(
      updater, std::move(callback));
  if (HRESULT hr = updater->RegisterApp(
          app_id.c_str(), brand_code.c_str(), ap.c_str(), version.c_str(),
          existence_checker_path.c_str(), callback_wrapper.Get());
      FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::RegisterApp" << std::hex << hr;
    callback_wrapper->Disconnect().Run(RegistrationResponse(hr));
    return;
  }
}

void UpdateServiceProxy::GetAppStatesSTA(
    base::OnceCallback<void(const std::vector<AppState>&)> callback,
    HRESULT /*prev_hr*/) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  // TODO(crbug.com/1094024): implement this feature in the COM server and then
  // replace this stub code with the actual call.
  std::move(callback).Run(std::vector<AppState>());
}

void UpdateServiceProxy::RunPeriodicTasksOnSTA(base::OnceClosure callback,
                                               HRESULT prev_hr) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  if (FAILED(prev_hr)) {
    std::move(callback).Run();
    return;
  }
  Microsoft::WRL::ComPtr<IUpdater> updater;
  if (HRESULT hr = CreateUpdater(scope_, updater); FAILED(hr)) {
    std::move(callback).Run();
    return;
  }
  auto callback_wrapper = Microsoft::WRL::Make<UpdaterCallback>(
      updater,
      base::BindOnce([](base::OnceClosure callback,
                        LONG /*status_code*/) { std::move(callback).Run(); },
                     std::move(callback)));
  if (HRESULT hr = updater->RunPeriodicTasks(callback_wrapper.Get());
      FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::RunPeriodicTasks" << std::hex << hr;
    callback_wrapper->Disconnect().Run(hr);
    return;
  }
}

void UpdateServiceProxy::UpdateAllOnSTA(StateChangeCallback state_update,
                                        Callback callback,
                                        HRESULT prev_hr) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  if (FAILED(prev_hr)) {
    std::move(callback).Run(Result::kServiceFailed);
    return;
  }
  Microsoft::WRL::ComPtr<IUpdater> updater;
  if (HRESULT hr = CreateUpdater(scope_, updater); FAILED(hr)) {
    std::move(callback).Run(Result::kServiceFailed);
    return;
  }

  // The COM RPC takes ownership of the `observer` and owns a reference to
  // the `updater` object as well. As long as the `observer` retains this
  // reference to the `updater` object, then the object is going to stay alive.
  // Once the server has notified, then released its last reference to the
  // `observer` object, the `observer` is destroyed, and as a result, the
  // last reference to `updater` is released as well, which causes the
  // destruction of the `updater` object.
  auto observer = Microsoft::WRL::Make<UpdaterObserver>(updater, state_update,
                                                        std::move(callback));
  if (HRESULT hr = updater->UpdateAll(observer.Get()); FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::UpdateAll" << std::hex << hr;

    // Since the RPC call returned an error, it can't be determined what the
    // state of the update server is. The observer may or may not post any
    // callback. Disconnecting the observer resolves this ambiguity and
    // transfers the ownership of the callback back to the owner of the
    // observer.
    observer->Disconnect().Run(Result::kServiceFailed);
    return;
  }
}

void UpdateServiceProxy::UpdateOnSTA(
    const std::string& app_id,
    const std::string& install_data_index,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback,
    HRESULT prev_hr) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  if (FAILED(prev_hr)) {
    std::move(callback).Run(Result::kServiceFailed);
    return;
  }
  Microsoft::WRL::ComPtr<IUpdater> updater;
  if (HRESULT hr = CreateUpdater(scope_, updater); FAILED(hr)) {
    std::move(callback).Run(Result::kServiceFailed);
    return;
  }
  auto observer = Microsoft::WRL::Make<UpdaterObserver>(updater, state_update,
                                                        std::move(callback));
  HRESULT hr =
      updater->Update(base::UTF8ToWide(app_id).c_str(),
                      base::UTF8ToWide(install_data_index).c_str(),
                      policy_same_version_update ==
                          UpdateService::PolicySameVersionUpdate::kAllowed,
                      observer.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::UpdateAll: " << std::hex << hr;
    observer->Disconnect().Run(Result::kServiceFailed);
    return;
  }
}

}  // namespace updater
