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

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"

namespace updater {
namespace {

using IUpdateStatePtr = ::Microsoft::WRL::ComPtr<IUpdateState>;
using ICompleteStatusPtr = ::Microsoft::WRL::ComPtr<ICompleteStatus>;

static constexpr base::TaskTraits kComClientTraits = {
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// Creates an instance of IUpdater in the COM STA apartment.
HRESULT CreateUpdater(Microsoft::WRL::ComPtr<IUpdater>& updater) {
  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(__uuidof(UpdaterClass), nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to instantiate the update server: " << std::hex << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IUpdater> updater_local;
  hr = server.As(&updater_local);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to query the updater interface: " << std::hex << hr;
    return hr;
  }

  updater = updater_local;
  return S_OK;
}

// This class implements the IUpdaterObserver interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread. However, its
// functions are invoked directly by COM RPC, and they are not sequenced through
// the thread task runner. This means that sequence checkers can't be used in
// this class.
class UpdaterObserver
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterObserver> {
 public:
  UpdaterObserver(Microsoft::WRL::ComPtr<IUpdater> updater,
                  UpdateService::StateChangeCallback state_update_callback,
                  UpdateService::Callback callback);
  UpdaterObserver(const UpdaterObserver&) = delete;
  UpdaterObserver& operator=(const UpdaterObserver&) = delete;

  // Overrides for IUpdaterObserver. These functions are called on the STA
  // thread directly by the COM RPC runtime.
  //
  // The implementation of this function queries the data in the `update_state`
  // object, then post a callback through the `com_task_runner_` to sequence
  // the execution of the COM code and the rest of the code in this module.
  // The `update_state` object is queried before returning the execution
  // flow back to the RPC channel, otherwise the RPC server keeps sending
  // state change notifications which queue up in the `com_task_runner_`.
  IFACEMETHODIMP OnStateChange(IUpdateState* update_state) override {
    DCHECK(update_state);
    com_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdaterObserver::OnStateChangeOnSTA,
                                  base::WrapRefCounted(this),
                                  QueryUpdateState(update_state)));
    return S_OK;
  }

  // See the comment above.
  IFACEMETHODIMP OnComplete(ICompleteStatus* complete_status) override {
    DCHECK(complete_status);
    com_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&UpdaterObserver::OnCompleteOnSTA,
                                              base::WrapRefCounted(this),
                                              QueryResult(complete_status)));
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  UpdateService::Callback Disconnect();

 private:
  ~UpdaterObserver() override;

  // Called in sequence on the `com_task_runner_`.
  void OnStateChangeOnSTA(
      const UpdateService::UpdateState& update_service_state);
  void OnCompleteOnSTA(const UpdateService::Result& result);

  UpdateService::UpdateState QueryUpdateState(IUpdateState* update_state);
  UpdateService::Result QueryResult(ICompleteStatus* complete_status);

  // Bound to the STA thread.
  THREAD_CHECKER(thread_checker_);

  // Bound to the STA thread.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  // Called by IUpdaterObserver::OnStateChange when update state change occur.
  UpdateService::StateChangeCallback state_update_callback_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::Callback callback_;
};

// This class implements the IUpdaterRegisterAppCallback interface and exposes
// it as a COM object. The class has thread-affinity for the STA thread.
// However, its functions are invoked directly by COM RPC, and they are not
// sequenced through the thread task runner. This means that sequence checkers
// can't be used in this class.
class UpdaterRegisterAppCallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterRegisterAppCallback> {
 public:
  UpdaterRegisterAppCallback(Microsoft::WRL::ComPtr<IUpdater> updater,
                             UpdateService::RegisterAppCallback callback);
  UpdaterRegisterAppCallback(const UpdaterRegisterAppCallback&) = delete;
  UpdaterRegisterAppCallback& operator=(const UpdaterRegisterAppCallback&) =
      delete;

  // Overrides for IUpdaterRegisterAppCallback. These functions are called on
  // the STA thread directly by the COM RPC runtime.
  IFACEMETHODIMP Run(LONG status_code) override {
    com_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdaterRegisterAppCallback::OnRunOnSTA,
                                  base::WrapRefCounted(this), status_code));
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  UpdateService::RegisterAppCallback Disconnect();

 private:
  ~UpdaterRegisterAppCallback() override;

  // Called in sequence on the `com_task_runner_`.
  void OnRunOnSTA(LONG status_code);

  // Bound to the STA thread.
  THREAD_CHECKER(thread_checker_);

  // Bound to the STA thread.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::RegisterAppCallback callback_;
};

// This class implements the IUpdaterCallback interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread.  However, its
// functions are invoked directly by COM RPC, and they are not sequenced through
// the thread task runner. This means that sequence checkers can't be used in
// this class.
class UpdaterCallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterCallback> {
 public:
  UpdaterCallback(Microsoft::WRL::ComPtr<IUpdater> updater,
                  base::OnceCallback<void(LONG)> callback);
  UpdaterCallback(const UpdaterCallback&) = delete;
  UpdaterCallback& operator=(const UpdaterCallback&) = delete;

  // Overrides for IUpdaterCallback. These functions are called on
  // the STA thread directly by the COM RPC runtime.
  IFACEMETHODIMP Run(LONG status_code) override {
    com_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdaterCallback::OnRunOnSTA,
                                  base::WrapRefCounted(this), status_code));
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(LONG)> Disconnect();

 private:
  ~UpdaterCallback() override;

  // Called in sequence on the `com_task_runner_`.
  void OnRunOnSTA(LONG status_code);

  // Bound to the STA thread.
  THREAD_CHECKER(thread_checker_);

  // Bound to the STA thread.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // Keeps a reference of the updater object alive, while this object is
  // owned by the COM RPC runtime.
  Microsoft::WRL::ComPtr<IUpdater> updater_;

  base::OnceCallback<void(LONG)> callback_;
};

}  // namespace

UpdaterObserver::UpdaterObserver(
    Microsoft::WRL::ComPtr<IUpdater> updater,
    UpdateService::StateChangeCallback state_update_callback,
    UpdateService::Callback callback)
    : com_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      updater_(updater),
      state_update_callback_(state_update_callback),
      callback_(std::move(callback)) {}

UpdaterObserver::~UpdaterObserver() = default;

UpdateService::Callback UpdaterObserver::Disconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  updater_ = nullptr;
  state_update_callback_.Reset();
  return std::move(callback_);
}

UpdateService::UpdateState UpdaterObserver::QueryUpdateState(
    IUpdateState* update_state) {
  DCHECK(update_state);
  UpdateService::UpdateState update_service_state;
  {
    LONG val_state = 0;
    HRESULT hr = update_state->get_state(&val_state);
    if (SUCCEEDED(hr)) {
      using State = UpdateService::UpdateState::State;
      base::Optional<State> state = CheckedCastToEnum<State>(val_state);
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
      base::Optional<ErrorCategory> error_category =
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

UpdateService::Result UpdaterObserver::QueryResult(
    ICompleteStatus* complete_status) {
  DCHECK(complete_status);

  LONG code = 0;
  base::win::ScopedBstr message;
  CHECK(SUCCEEDED(complete_status->get_statusCode(&code)));

  DVLOG(2) << "ICompleteStatus::OnComplete(" << code << ")";
  return static_cast<UpdateService::Result>(code);
}

void UpdaterObserver::OnStateChangeOnSTA(
    const UpdateService::UpdateState& update_service_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DVLOG(4) << __func__;

  if (!state_update_callback_) {
    DVLOG(4) << "Skipping posting the update state callback.";
    return;
  }

  com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(state_update_callback_, update_service_state));
}

void UpdaterObserver::OnCompleteOnSTA(const UpdateService::Result& result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  updater_ = nullptr;

  if (!callback_) {
    DVLOG(2) << "Skipping posting the completion callback.";
    return;
  }
  com_task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback_), result));
}

UpdaterRegisterAppCallback::UpdaterRegisterAppCallback(
    Microsoft::WRL::ComPtr<IUpdater> updater,
    UpdateService::RegisterAppCallback callback)
    : com_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      updater_(updater),
      callback_(std::move(callback)) {}

UpdaterRegisterAppCallback::~UpdaterRegisterAppCallback() = default;

UpdateService::RegisterAppCallback UpdaterRegisterAppCallback::Disconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  updater_ = nullptr;
  return std::move(callback_);
}

void UpdaterRegisterAppCallback::OnRunOnSTA(LONG status_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DVLOG(4) << __func__;

  if (!callback_) {
    DVLOG(4) << "Skipping posting the register app callback.";
    return;
  }

  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), RegistrationResponse(status_code)));
}

UpdaterCallback::UpdaterCallback(Microsoft::WRL::ComPtr<IUpdater> updater,
                                 base::OnceCallback<void(LONG)> callback)
    : com_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      updater_(updater),
      callback_(std::move(callback)) {}

UpdaterCallback::~UpdaterCallback() = default;

base::OnceCallback<void(LONG)> UpdaterCallback::Disconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  updater_ = nullptr;
  return std::move(callback_);
}

void UpdaterCallback::OnRunOnSTA(LONG status_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DVLOG(4) << __func__;

  if (!callback_) {
    DVLOG(4) << "Skipping posting the callback.";
    return;
  }

  com_task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback_), status_code));
}

UpdateServiceProxy::UpdateServiceProxy(UpdaterScope updater_scope)
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      com_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner(kComClientTraits)) {
  DCHECK_EQ(updater_scope, UpdaterScope::kUser);
}

UpdateServiceProxy::~UpdateServiceProxy() = default;

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateServiceProxy::GetVersionOnSTA, this,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 base::OnceCallback<void(const base::Version&)> callback,
                 const base::Version& version) {
                taskrunner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), version));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     RegisterAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reposts the call to the COM task runner. Adapts `callback` so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateServiceProxy::RegisterAppOnSTA, this, request,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 RegisterAppCallback callback,
                 const RegistrationResponse& response) {
                taskrunner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), response));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateServiceProxy::RunPeriodicTasksOnSTA, this,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(callback))));
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reposts the call to the COM task runner. Adapts `callback` so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateServiceProxy::UpdateAllOnSTA, this, state_update,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 Callback callback, Result result) {
                taskrunner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void UpdateServiceProxy::Update(const std::string& app_id,
                                UpdateService::Priority /*priority*/,
                                StateChangeCallback state_update,
                                Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reposts the call to the COM task runner. Adapts `callback` so that
  // the callback runs on the main sequence.
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateServiceProxy::UpdateOnSTA, this, app_id,
          base::BindRepeating(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 StateChangeCallback state_update, UpdateState update_state) {
                taskrunner->PostTask(
                    FROM_HERE, base::BindRepeating(state_update, update_state));
              },
              base::SequencedTaskRunnerHandle::Get(), state_update),
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> taskrunner,
                 Callback callback, Result result) {
                taskrunner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void UpdateServiceProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpdateServiceProxy::GetVersionOnSTA(
    base::OnceCallback<void(const base::Version&)> callback) const {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUpdater> updater;
  HRESULT hr = CreateUpdater(updater);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create the updater interface: " << std::hex << hr;
    std::move(callback).Run(base::Version());
    return;
  }

  base::win::ScopedBstr version;
  hr = updater->GetVersion(version.Receive());
  if (FAILED(hr)) {
    DVLOG(2) << "IUpdater::GetVersion failed: " << std::hex << hr;
    std::move(callback).Run(base::Version());
    return;
  }

  std::move(callback).Run(base::Version(base::WideToUTF8(version.Get())));
}

void UpdateServiceProxy::RegisterAppOnSTA(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUpdater> updater;
  HRESULT hr = CreateUpdater(updater);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create the updater interface: " << std::hex << hr;
    std::move(callback).Run(RegistrationResponse(hr));
    return;
  }

  std::wstring app_id;
  std::wstring brand_code;
  std::wstring tag;
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
        if (!base::UTF8ToWide(request.tag.c_str(), request.tag.size(), &tag)) {
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
    std::move(callback).Run(RegistrationResponse(-1));
    return;
  }

  auto callback_wrapper = Microsoft::WRL::Make<UpdaterRegisterAppCallback>(
      updater, std::move(callback));
  hr = updater->RegisterApp(app_id.c_str(), brand_code.c_str(), tag.c_str(),
                            version.c_str(), existence_checker_path.c_str(),
                            callback_wrapper.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::RegisterApp" << std::hex << hr;
    callback_wrapper->Disconnect().Run(RegistrationResponse(hr));
    return;
  }
}

void UpdateServiceProxy::RunPeriodicTasksOnSTA(base::OnceClosure callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());
  Microsoft::WRL::ComPtr<IUpdater> updater;
  HRESULT hr = CreateUpdater(updater);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create the updater interface: " << std::hex << hr;
    std::move(callback).Run();
    return;
  }

  auto callback_wrapper = Microsoft::WRL::Make<UpdaterCallback>(
      updater, base::BindOnce([](base::OnceClosure callback,
                                 LONG unused) { std::move(callback).Run(); },
                              std::move(callback)));
  hr = updater->RunPeriodicTasks(callback_wrapper.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::RunPeriodicTasks" << std::hex << hr;
    callback_wrapper->Disconnect().Run(hr);
    return;
  }
}

void UpdateServiceProxy::UpdateAllOnSTA(StateChangeCallback state_update,
                                        Callback callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUpdater> updater;
  HRESULT hr = CreateUpdater(updater);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create the updater interface: " << std::hex << hr;
    std::move(callback).Run(Result::kServiceFailed);
    return;
  }

  // The COM RPC takes ownership of the `observer` and owns a reference to
  // the updater object as well. As long as the `observer` retains this
  // reference to the updater object, then the object is going to stay alive.
  // The `observer` can drop its reference to the updater object after
  // handling the last server callback, then the object model is torn down,
  // and finally, the execution flow returns back into the App object once the
  // completion callback is posted.
  auto observer = Microsoft::WRL::Make<UpdaterObserver>(updater, state_update,
                                                        std::move(callback));
  hr = updater->UpdateAll(observer.Get());
  if (FAILED(hr)) {
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

void UpdateServiceProxy::UpdateOnSTA(const std::string& app_id,
                                     StateChangeCallback state_update,
                                     Callback callback) {
  DCHECK(com_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IUpdater> updater;
  HRESULT hr = CreateUpdater(updater);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create the updater interface: " << std::hex << hr;
    std::move(callback).Run(Result::kServiceFailed);
    return;
  }

  auto observer = Microsoft::WRL::Make<UpdaterObserver>(updater, state_update,
                                                        std::move(callback));
  hr = updater->Update(base::UTF8ToWide(app_id).c_str(), observer.Get());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to call IUpdater::UpdateAll: " << std::hex << hr;

    // See the comment in the implementation of `UpdateAllOnSTA`.
    observer->Disconnect().Run(Result::kServiceFailed);
    return;
  }
}

}  // namespace updater
