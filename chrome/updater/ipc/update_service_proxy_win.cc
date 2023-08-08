// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_win.h"

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/ipc/proxy_impl_base_win.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

using IUpdateStatePtr = ::Microsoft::WRL::ComPtr<IUpdateState>;
using ICompleteStatusPtr = ::Microsoft::WRL::ComPtr<ICompleteStatus>;

// This class implements the IUpdaterObserver interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread.
class UpdaterObserver : public DYNAMICIIDSIMPL(IUpdaterObserver) {
 public:
  UpdaterObserver(UpdateService::StateChangeCallback state_update_callback,
                  UpdateService::Callback callback)
      : state_update_callback_(state_update_callback),
        callback_(std::move(callback)) {}
  UpdaterObserver(const UpdaterObserver&) = delete;
  UpdaterObserver& operator=(const UpdaterObserver&) = delete;

  // Overrides for IUpdaterObserver. These functions are called on the STA
  // thread directly by the COM RPC runtime.
  IFACEMETHODIMP OnStateChange(IUpdateState* update_state) override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    CHECK(update_state);

    if (!state_update_callback_) {
      VLOG(2) << "Skipping posting the update state callback.";
      return S_OK;
    }

    state_update_callback_.Run(QueryUpdateState(update_state));
    return S_OK;
  }

  IFACEMETHODIMP OnComplete(ICompleteStatus* complete_status) override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    CHECK(complete_status);
    result_ = QueryResult(complete_status);
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  UpdateService::Callback Disconnect() {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    VLOG(2) << __func__;
    state_update_callback_.Reset();
    return std::move(callback_);
  }

 private:
  ~UpdaterObserver() override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run(result_);
  }

  UpdateService::UpdateState QueryUpdateState(IUpdateState* update_state) {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    CHECK(update_state);

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
    {
      base::win::ScopedBstr installer_text;
      HRESULT hr = update_state->get_installerText(installer_text.Receive());
      if (SUCCEEDED(hr)) {
        update_service_state.installer_text =
            base::WideToUTF8(installer_text.Get());
      }
    }
    {
      base::win::ScopedBstr installer_cmd_line;
      HRESULT hr =
          update_state->get_installerCommandLine(installer_cmd_line.Receive());
      if (SUCCEEDED(hr)) {
        update_service_state.installer_cmd_line =
            base::WideToUTF8(installer_cmd_line.Get());
      }
    }

    VLOG(4) << update_service_state;
    return update_service_state;
  }

  UpdateService::Result QueryResult(ICompleteStatus* complete_status) {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    CHECK(complete_status);

    LONG code = 0;
    base::win::ScopedBstr message;
    CHECK(SUCCEEDED(complete_status->get_statusCode(&code)));

    VLOG(2) << "ICompleteStatus::OnComplete(" << code << ")";
    return static_cast<UpdateService::Result>(code);
  }

  // The reference of the thread this object is bound to.
  const base::PlatformThreadRef com_thread_ref_ =
      base::PlatformThread::CurrentRef();

  // Called by IUpdaterObserver::OnStateChange when update state changes occur.
  UpdateService::StateChangeCallback state_update_callback_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  UpdateService::Callback callback_;

  UpdateService::Result result_ = UpdateService::Result::kSuccess;
};

// This class implements the IUpdaterCallback interface and exposes it as a COM
// object. The class has thread-affinity for the STA thread.
class UpdaterCallback : public DYNAMICIIDSIMPL(IUpdaterCallback) {
 public:
  explicit UpdaterCallback(base::OnceCallback<void(LONG)> callback)
      : callback_(std::move(callback)) {}
  UpdaterCallback(const UpdaterCallback&) = delete;
  UpdaterCallback& operator=(const UpdaterCallback&) = delete;

  // Overrides for IUpdaterCallback. This function is called on the STA
  // thread directly by the COM RPC runtime, and must be sequenced through
  // the task runner.
  IFACEMETHODIMP Run(LONG status_code) override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    VLOG(2) << __func__;
    status_code_ = status_code;
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(LONG)> Disconnect() {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    VLOG(2) << __func__;
    return std::move(callback_);
  }

 private:
  ~UpdaterCallback() override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    if (callback_)
      std::move(callback_).Run(status_code_);
  }

  // The reference of the thread this object is bound to.
  const base::PlatformThreadRef com_thread_ref_ =
      base::PlatformThread::CurrentRef();

  base::OnceCallback<void(LONG)> callback_;

  LONG status_code_ = 0;
};

// This class implements the IUpdaterAppStatesCallback interface and exposes it
// as a COM object. The class has thread-affinity for the STA thread.
class UpdaterAppStatesCallback
    : public DYNAMICIIDSIMPL(IUpdaterAppStatesCallback) {
 public:
  explicit UpdaterAppStatesCallback(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
          callback)
      : callback_(std::move(callback)) {}
  UpdaterAppStatesCallback(const UpdaterAppStatesCallback&) = delete;
  UpdaterAppStatesCallback& operator=(const UpdaterAppStatesCallback&) = delete;

  // Overrides for IUpdaterAppStatesCallback. This function is called on the STA
  // thread directly by the COM RPC runtime, and must be sequenced through
  // the task runner.
  IFACEMETHODIMP Run(VARIANT updater_app_states) override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    VLOG(2) << __func__;

    if (V_VT(&updater_app_states) != (VT_ARRAY | VT_DISPATCH)) {
      return E_INVALIDARG;
    }

    // The safearray is owned by the caller of `Run`, so ownership is released
    // here after acquiring the `LockScope`.
    base::win::ScopedSafearray safearray(V_ARRAY(&updater_app_states));
    absl::optional<base::win::ScopedSafearray::LockScope<VT_DISPATCH>>
        lock_scope = safearray.CreateLockScope<VT_DISPATCH>();
    safearray.Release();

    if (!lock_scope.has_value() || !lock_scope->size()) {
      return E_INVALIDARG;
    }

    for (size_t i = 0; i < lock_scope->size(); ++i) {
      Microsoft::WRL::ComPtr<IDispatch> dispatch(lock_scope->at(i));
      if (!dispatch) {
        return E_INVALIDARG;
      }
      Microsoft::WRL::ComPtr<IUpdaterAppState> app_state;
      const HRESULT hr =
          dispatch.CopyTo(IsSystemInstall() ? __uuidof(IUpdaterAppStateSystem)
                                            : __uuidof(IUpdaterAppStateUser),
                          IID_PPV_ARGS_Helper(&app_state));
      if (FAILED(hr)) {
        return hr;
      }
      app_states_.push_back(IUpdaterAppStateToAppState(app_state));
    }

    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
  Disconnect() {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    VLOG(2) << __func__;
    return std::move(callback_);
  }

 private:
  ~UpdaterAppStatesCallback() override {
    CHECK_EQ(base::PlatformThread::CurrentRef(), com_thread_ref_);
    if (callback_) {
      std::move(callback_).Run(app_states_);
    }
  }

  UpdateService::AppState IUpdaterAppStateToAppState(
      Microsoft::WRL::ComPtr<IUpdaterAppState> updater_app_state) {
    DCHECK(updater_app_state);

    UpdateService::AppState app_state;
    {
      base::win::ScopedBstr app_id;
      HRESULT hr = updater_app_state->get_appId(app_id.Receive());
      if (SUCCEEDED(hr)) {
        app_state.app_id = base::WideToUTF8(app_id.Get());
      }
    }
    {
      base::win::ScopedBstr version;
      HRESULT hr = updater_app_state->get_version(version.Receive());
      if (SUCCEEDED(hr)) {
        app_state.version = base::Version(base::WideToUTF8(version.Get()));
      }
    }
    {
      base::win::ScopedBstr ap;
      HRESULT hr = updater_app_state->get_ap(ap.Receive());
      if (SUCCEEDED(hr)) {
        app_state.ap = base::WideToUTF8(ap.Get());
      }
    }
    {
      base::win::ScopedBstr brand_code;
      HRESULT hr = updater_app_state->get_brandCode(brand_code.Receive());
      if (SUCCEEDED(hr)) {
        app_state.brand_code = base::WideToUTF8(brand_code.Get());
      }
    }
    {
      base::win::ScopedBstr brand_path;
      HRESULT hr = updater_app_state->get_brandPath(brand_path.Receive());
      if (SUCCEEDED(hr)) {
        app_state.brand_path = base::FilePath(brand_path.Get());
      }
    }
    {
      base::win::ScopedBstr ecp;
      HRESULT hr = updater_app_state->get_ecp(ecp.Receive());
      if (SUCCEEDED(hr)) {
        app_state.ecp = base::FilePath(ecp.Get());
      }
    }

    return app_state;
  }

  // The reference of the thread this object is bound to.
  const base::PlatformThreadRef com_thread_ref_ =
      base::PlatformThread::CurrentRef();

  base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
      callback_;

  std::vector<UpdateService::AppState> app_states_;
};

}  // namespace

class UpdateServiceProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImpl>,
      public ProxyImplBase<UpdateServiceProxyImpl,
                           IUpdater,
                           __uuidof(IUpdaterUser),
                           __uuidof(IUpdaterSystem)> {
 public:
  explicit UpdateServiceProxyImpl(UpdaterScope scope) : ProxyImplBase(scope) {}

  static auto GetClassGuid(UpdaterScope scope) {
    return IsSystemInstall(scope) ? __uuidof(UpdaterSystemClass)
                                  : __uuidof(UpdaterUserClass);
  }

  void GetVersion(base::OnceCallback<void(const base::Version&)> callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::GetVersionOnSTA, this,
                               std::move(callback)));
  }

  void FetchPolicies(base::OnceCallback<void(int)> callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::FetchPoliciesOnSTA,
                               this, std::move(callback)));
  }

  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::RegisterAppOnSTA, this,
                               request, std::move(callback)));
  }

  void GetAppStates(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
          callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::GetAppStatesOnSTA, this,
                               std::move(callback)));
  }

  void RunPeriodicTasks(base::OnceClosure callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::RunPeriodicTasksOnSTA,
                               this, std::move(callback)));
  }

  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      UpdateService::StateChangeCallback state_update,
      UpdateService::Callback callback) {
    PostRPCTask(base::BindOnce(
        &UpdateServiceProxyImpl::CheckForUpdateOnSTA, this, app_id, priority,
        policy_same_version_update, state_update, std::move(callback)));
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index,
              UpdateService::Priority priority,
              UpdateService::PolicySameVersionUpdate policy_same_version_update,
              UpdateService::StateChangeCallback state_update,
              UpdateService::Callback callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::UpdateOnSTA, this,
                               app_id, install_data_index, priority,
                               policy_same_version_update, state_update,
                               std::move(callback)));
  }

  void UpdateAll(UpdateService::StateChangeCallback state_update,
                 UpdateService::Callback callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::UpdateAllOnSTA, this,
                               state_update, std::move(callback)));
  }

  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               UpdateService::Priority priority,
               UpdateService::StateChangeCallback state_update,
               UpdateService::Callback callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::InstallOnSTA, this,
                               registration, client_install_data,
                               install_data_index, priority, state_update,
                               std::move(callback)));
  }

  void CancelInstalls(const std::string& app_id) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::CancelInstallsOnSTA,
                               this, app_id));
  }

  void RunInstaller(const std::string& app_id,
                    const base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    UpdateService::StateChangeCallback state_update,
                    UpdateService::Callback callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImpl::RunInstallerOnSTA, this,
                               app_id, installer_path, install_args,
                               install_data, install_settings, state_update,
                               std::move(callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImpl>;
  virtual ~UpdateServiceProxyImpl() = default;

  void GetVersionOnSTA(
      base::OnceCallback<void(const base::Version&)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(base::Version());
      return;
    }
    base::win::ScopedBstr version;
    if (HRESULT hr = get_interface()->GetVersion(version.Receive());
        FAILED(hr)) {
      VLOG(2) << "IUpdater::GetVersion failed: " << std::hex << hr;
      std::move(callback).Run(base::Version());
      return;
    }
    std::move(callback).Run(base::Version(base::WideToUTF8(version.Get())));
  }

  void FetchPoliciesOnSTA(base::OnceCallback<void(int)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(hresult());
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterCallback>(base::BindOnce(
            [](base::OnceCallback<void(int)> callback, LONG status_code) {
              std::move(callback).Run(status_code);
            },
            std::move(callback)));
    if (HRESULT hr = get_interface()->FetchPolicies(callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::FetchPolicies, " << std::hex << hr;
      callback_wrapper->Disconnect().Run(hr);
      return;
    }
  }

  void RegisterAppOnSTA(const RegistrationRequest& request,
                        base::OnceCallback<void(int)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(hresult());
      return;
    }
    std::wstring app_id_w;
    std::wstring brand_code_w;
    std::wstring brand_path_w;
    std::wstring ap_w;
    std::wstring version_w;
    std::wstring existence_checker_path_w;
    if (![&]() {
          if (!base::UTF8ToWide(request.app_id.c_str(), request.app_id.size(),
                                &app_id_w)) {
            return false;
          }
          if (!base::UTF8ToWide(request.brand_code.c_str(),
                                request.brand_code.size(), &brand_code_w)) {
            return false;
          }
          brand_path_w = request.brand_path.value();
          if (!base::UTF8ToWide(request.ap.c_str(), request.ap.size(), &ap_w)) {
            return false;
          }
          std::string version_str = request.version.GetString();
          if (!base::UTF8ToWide(version_str.c_str(), version_str.size(),
                                &version_w)) {
            return false;
          }
          existence_checker_path_w = request.existence_checker_path.value();
          return true;
        }()) {
      std::move(callback).Run(E_INVALIDARG);
      return;
    }

    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterCallback>(base::BindOnce(
            [](base::OnceCallback<void(int)> callback, LONG status_code) {
              std::move(callback).Run(status_code);
            },
            std::move(callback)));
    if (HRESULT hr = get_interface()->RegisterApp(
            app_id_w.c_str(), brand_code_w.c_str(), brand_path_w.c_str(),
            ap_w.c_str(), version_w.c_str(), existence_checker_path_w.c_str(),
            callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::RegisterApp" << std::hex << hr;
      callback_wrapper->Disconnect().Run(hr);
      return;
    }
  }

  void GetAppStatesOnSTA(
      base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run({});
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterAppStatesCallback>(std::move(callback));
    if (HRESULT hr = get_interface()->GetAppStates(callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::GetAppStates, " << std::hex << hr;
      callback_wrapper->Disconnect().Run({});
      return;
    }
  }

  void RunPeriodicTasksOnSTA(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run();
      return;
    }
    auto callback_wrapper = MakeComObjectOrCrash<UpdaterCallback>(
        base::BindOnce([](base::OnceClosure callback,
                          LONG /*status_code*/) { std::move(callback).Run(); },
                       std::move(callback)));
    if (HRESULT hr = get_interface()->RunPeriodicTasks(callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::RunPeriodicTasks" << std::hex << hr;
      callback_wrapper->Disconnect().Run(hr);
      return;
    }
  }

  void CheckForUpdateOnSTA(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      UpdateService::StateChangeCallback state_update,
      UpdateService::Callback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }
    std::wstring app_id_w;
    if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &app_id_w)) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }

    auto observer = MakeComObjectOrCrash<UpdaterObserver>(state_update,
                                                          std::move(callback));
    HRESULT hr = get_interface()->CheckForUpdate(
        app_id_w.c_str(), static_cast<int>(priority),
        policy_same_version_update ==
            UpdateService::PolicySameVersionUpdate::kAllowed,
        observer.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::CheckForUpdate: " << std::hex << hr;
      observer->Disconnect().Run(UpdateService::Result::kServiceFailed);
      return;
    }
  }

  void UpdateOnSTA(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      UpdateService::StateChangeCallback state_update,
      UpdateService::Callback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }
    std::wstring app_id_w;
    std::wstring install_data_index_w;
    if (![&]() {
          if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &app_id_w)) {
            return false;
          }
          if (!base::UTF8ToWide(install_data_index.c_str(),
                                install_data_index.size(),
                                &install_data_index_w)) {
            return false;
          }
          return true;
        }()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }

    auto observer = MakeComObjectOrCrash<UpdaterObserver>(state_update,
                                                          std::move(callback));
    HRESULT hr = get_interface()->Update(
        app_id_w.c_str(), install_data_index_w.c_str(),
        static_cast<int>(priority),
        policy_same_version_update ==
            UpdateService::PolicySameVersionUpdate::kAllowed,
        observer.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::Update: " << std::hex << hr;
      observer->Disconnect().Run(UpdateService::Result::kServiceFailed);
      return;
    }
  }

  void UpdateAllOnSTA(UpdateService::StateChangeCallback state_update,
                      UpdateService::Callback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }
    auto observer = MakeComObjectOrCrash<UpdaterObserver>(state_update,
                                                          std::move(callback));
    if (HRESULT hr = get_interface()->UpdateAll(observer.Get()); FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::UpdateAll" << std::hex << hr;
      observer->Disconnect().Run(UpdateService::Result::kServiceFailed);
      return;
    }
  }

  void InstallOnSTA(const RegistrationRequest& request,
                    const std::string& client_install_data,
                    const std::string& install_data_index,
                    UpdateService::Priority priority,
                    UpdateService::StateChangeCallback state_update,
                    UpdateService::Callback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }
    std::wstring app_id_w;
    std::wstring brand_code_w;
    std::wstring brand_path_w;
    std::wstring ap_w;
    std::wstring version_w;
    std::wstring existence_checker_path_w;
    std::wstring client_install_data_w;
    std::wstring install_data_index_w;
    if (![&]() {
          if (!base::UTF8ToWide(request.app_id.c_str(), request.app_id.size(),
                                &app_id_w)) {
            return false;
          }
          if (!base::UTF8ToWide(request.brand_code.c_str(),
                                request.brand_code.size(), &brand_code_w)) {
            return false;
          }
          brand_path_w = request.brand_path.value();
          if (!base::UTF8ToWide(request.ap.c_str(), request.ap.size(), &ap_w)) {
            return false;
          }
          std::string version_str = request.version.GetString();
          if (!base::UTF8ToWide(version_str.c_str(), version_str.size(),
                                &version_w)) {
            return false;
          }
          existence_checker_path_w = request.existence_checker_path.value();
          if (!base::UTF8ToWide(client_install_data.c_str(),
                                client_install_data.size(),
                                &client_install_data_w)) {
            return false;
          }
          if (!base::UTF8ToWide(install_data_index.c_str(),
                                install_data_index.size(),
                                &install_data_index_w)) {
            return false;
          }
          return true;
        }()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }
    auto observer = MakeComObjectOrCrash<UpdaterObserver>(state_update,
                                                          std::move(callback));
    HRESULT hr = get_interface()->Install(
        app_id_w.c_str(), brand_code_w.c_str(), brand_path_w.c_str(),
        ap_w.c_str(), version_w.c_str(), existence_checker_path_w.c_str(),
        client_install_data_w.c_str(), install_data_index_w.c_str(),
        static_cast<int>(priority), observer.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::Install: " << std::hex << hr;
      observer->Disconnect().Run(UpdateService::Result::kServiceFailed);
      return;
    }
  }

  void CancelInstallsOnSTA(const std::string& app_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ConnectToServer()) {
      return;
    }
    if (HRESULT hr =
            get_interface()->CancelInstalls(base::UTF8ToWide(app_id).c_str());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::CancelInstalls: " << std::hex << hr;
    }
  }

  void RunInstallerOnSTA(const std::string& app_id,
                         const base::FilePath& installer_path,
                         const std::string& install_args,
                         const std::string& install_data,
                         const std::string& install_settings,
                         UpdateService::StateChangeCallback state_update,
                         UpdateService::Callback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
    if (!ConnectToServer()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }
    std::wstring app_id_w;
    std::wstring install_args_w;
    std::wstring install_data_w;
    std::wstring install_settings_w;
    if (![&]() {
          if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &app_id_w)) {
            return false;
          }
          if (!base::UTF8ToWide(install_args.c_str(), install_args.size(),
                                &install_args_w)) {
            return false;
          }
          if (!base::UTF8ToWide(install_data.c_str(), install_data.size(),
                                &install_data_w)) {
            return false;
          }
          if (!base::UTF8ToWide(install_settings.c_str(),
                                install_settings.size(), &install_settings_w)) {
            return false;
          }
          return true;
        }()) {
      std::move(callback).Run(UpdateService::Result::kServiceFailed);
      return;
    }

    auto observer = MakeComObjectOrCrash<UpdaterObserver>(state_update,
                                                          std::move(callback));
    HRESULT hr = get_interface()->RunInstaller(
        app_id_w.c_str(), installer_path.value().c_str(),
        install_args_w.c_str(), install_data_w.c_str(),
        install_settings_w.c_str(), observer.Get());
    if (SUCCEEDED(hr)) {
      VLOG(2) << "IUpdater::OfflineInstall completed successfully.";
    } else {
      VLOG(2) << "Failed to call IUpdater::OfflineInstall: " << std::hex << hr;
      observer->Disconnect().Run(UpdateService::Result::kServiceFailed);
    }
  }
};

UpdateServiceProxy::UpdateServiceProxy(UpdaterScope updater_scope)
    : impl_(base::MakeRefCounted<UpdateServiceProxyImpl>(updater_scope)) {}

UpdateServiceProxy::~UpdateServiceProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  UpdateServiceProxyImpl::Destroy(impl_);
  CHECK_EQ(impl_, nullptr);
}

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->GetVersion(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &UpdateServiceProxy::GetVersionDone, this, std::move(callback))));
}

void UpdateServiceProxy::FetchPolicies(base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->FetchPolicies(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &UpdateServiceProxy::FetchPoliciesDone, this, std::move(callback))));
}

void UpdateServiceProxy::RegisterApp(const RegistrationRequest& request,
                                     base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RegisterApp(request, base::BindPostTaskToCurrentDefault(base::BindOnce(
                                  &UpdateServiceProxy::RegisterAppDone, this,
                                  std::move(callback))));
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->GetAppStates(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &UpdateServiceProxy::GetAppStatesDone, this, std::move(callback))));
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RunPeriodicTasks(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &UpdateServiceProxy::RunPeriodicTasksDone, this, std::move(callback))));
}

void UpdateServiceProxy::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->CheckForUpdate(
      app_id, priority, policy_same_version_update,
      base::BindPostTaskToCurrentDefault(state_update),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &UpdateServiceProxy::CheckForUpdateDone, this, std::move(callback))));
}

void UpdateServiceProxy::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Update(
      app_id, install_data_index, priority, policy_same_version_update,
      base::BindPostTaskToCurrentDefault(state_update),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &UpdateServiceProxy::UpdateDone, this, std::move(callback))));
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->UpdateAll(
      base::BindPostTaskToCurrentDefault(state_update),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &UpdateServiceProxy::UpdateAllDone, this, std::move(callback))));
}

void UpdateServiceProxy::Install(const RegistrationRequest& registration,
                                 const std::string& client_install_data,
                                 const std::string& install_data_index,
                                 Priority priority,
                                 StateChangeCallback state_update,
                                 Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Install(
      registration, client_install_data, install_data_index, priority,
      base::BindPostTaskToCurrentDefault(state_update),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &UpdateServiceProxy::InstallDone, this, std::move(callback))));
}

void UpdateServiceProxy::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->CancelInstalls(app_id);
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
  impl_->RunInstaller(
      app_id, installer_path, install_args, install_data, install_settings,
      base::BindPostTaskToCurrentDefault(state_update),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &UpdateServiceProxy::RunInstallerDone, this, std::move(callback))));
}

void UpdateServiceProxy::GetVersionDone(
    base::OnceCallback<void(const base::Version&)> callback,
    const base::Version& version) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(version);
}

void UpdateServiceProxy::FetchPoliciesDone(
    base::OnceCallback<void(int)> callback,
    int result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

void UpdateServiceProxy::RegisterAppDone(base::OnceCallback<void(int)> callback,
                                         int result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

void UpdateServiceProxy::GetAppStatesDone(
    base::OnceCallback<void(const std::vector<AppState>&)> callback,
    const std::vector<AppState>& results) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(results);
}

void UpdateServiceProxy::RunPeriodicTasksDone(base::OnceClosure callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void UpdateServiceProxy::CheckForUpdateDone(Callback callback, Result result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

void UpdateServiceProxy::UpdateDone(Callback callback, Result result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

void UpdateServiceProxy::UpdateAllDone(Callback callback, Result result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

void UpdateServiceProxy::InstallDone(Callback callback, Result result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

void UpdateServiceProxy::RunInstallerDone(Callback callback, Result result) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result);
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope,
    const base::TimeDelta& /*get_version_timeout*/) {
  return base::MakeRefCounted<UpdateServiceProxy>(updater_scope);
}

}  // namespace updater
