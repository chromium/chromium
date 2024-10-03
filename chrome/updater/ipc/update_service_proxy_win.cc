// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_win.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/ipc/proxy_impl_base_win.h"
#include "chrome/updater/ipc/update_service_proxy.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

class UpdaterObserver : public DYNAMICIIDSIMPL(IUpdaterObserver) {
 public:
  UpdaterObserver(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update_callback,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback)
      : state_update_callback_(state_update_callback),
        callback_(std::move(callback)) {}
  UpdaterObserver(const UpdaterObserver&) = delete;
  UpdaterObserver& operator=(const UpdaterObserver&) = delete;

  // Overrides for IUpdaterObserver. Called on a system thread by COM RPC.
  // Retries querying the update state two times, since runtime RPC errors when
  // calling IUpdateState members have been observed in production.
  IFACEMETHODIMP OnStateChange(IUpdateState* update_state) override {
    CHECK(update_state);
    if (!state_update_callback_) {
      VLOG(2) << "Skipping posting: no update state callback.";
      return S_OK;
    }
    for (int try_count = 0; try_count < 2; ++try_count) {
      HResultOr<UpdateService::UpdateState> service_state =
          QueryUpdateState(update_state);
      if (service_state.has_value()) {
        state_update_callback_.Run(*service_state);
        break;
      }
      VLOG(2) << "QueryUpdateState returned " << service_state.error();
    }
    return S_OK;
  }

  IFACEMETHODIMP OnComplete(ICompleteStatus* complete_status) override {
    CHECK(complete_status);
    result_ = QueryResult(complete_status);
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
  Disconnect() {
    VLOG(2) << __func__;
    state_update_callback_.Reset();
    return std::move(callback_);
  }

 private:
  ~UpdaterObserver() override {
    if (callback_) {
      std::move(callback_).Run(result_);
    }
  }

  static HResultOr<UpdateService::UpdateState> QueryUpdateState(
      IUpdateState* update_state) {
    CHECK(update_state);
    UpdateService::UpdateState update_service_state;
    {
      LONG val_state = 0;
      if (HRESULT hr = update_state->get_state(&val_state); FAILED(hr)) {
        return base::unexpected(hr);
      }
      using State = UpdateService::UpdateState::State;
      std::optional<State> state = CheckedCastToEnum<State>(val_state);
      if (!state) {
        return base::unexpected(E_INVALIDARG);
      }
      update_service_state.state = *state;
    }
    {
      base::win::ScopedBstr app_id;
      if (HRESULT hr = update_state->get_appId(app_id.Receive()); FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.app_id = base::WideToUTF8(app_id.Get());
    }
    {
      base::win::ScopedBstr next_version;
      if (HRESULT hr = update_state->get_nextVersion(next_version.Receive());
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.next_version =
          base::Version(base::WideToUTF8(next_version.Get()));
    }
    {
      LONGLONG downloaded_bytes = -1;
      if (HRESULT hr = update_state->get_downloadedBytes(&downloaded_bytes);
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.downloaded_bytes = downloaded_bytes;
    }
    {
      LONGLONG total_bytes = -1;
      if (HRESULT hr = update_state->get_totalBytes(&total_bytes); FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.total_bytes = total_bytes;
    }
    {
      LONG install_progress = -1;
      if (HRESULT hr = update_state->get_installProgress(&install_progress);
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.install_progress = install_progress;
    }
    {
      LONG val_error_category = 0;
      if (HRESULT hr = update_state->get_errorCategory(&val_error_category);
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      using ErrorCategory = UpdateService::ErrorCategory;
      std::optional<ErrorCategory> error_category =
          CheckedCastToEnum<ErrorCategory>(val_error_category);
      if (!error_category) {
        return base::unexpected(E_INVALIDARG);
      }
      update_service_state.error_category = *error_category;
    }
    {
      LONG error_code = -1;
      if (HRESULT hr = update_state->get_errorCode(&error_code); FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.error_code = error_code;
    }
    {
      LONG extra_code1 = -1;
      if (HRESULT hr = update_state->get_extraCode1(&extra_code1); FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.extra_code1 = extra_code1;
    }
    {
      base::win::ScopedBstr installer_text;
      if (HRESULT hr =
              update_state->get_installerText(installer_text.Receive());
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.installer_text =
          base::WideToUTF8(installer_text.Get());
    }
    {
      base::win::ScopedBstr installer_cmd_line;
      if (HRESULT hr = update_state->get_installerCommandLine(
              installer_cmd_line.Receive());
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      update_service_state.installer_cmd_line =
          base::WideToUTF8(installer_cmd_line.Get());
    }

    // TODO(crbug.com/345250525) - understand why the check fails.
    base::debug::Alias(&update_service_state);
    if (update_service_state.state ==
        UpdateService::UpdateState::State::kUnknown) {
      VLOG(2) << update_service_state;
      base::debug::DumpWithoutCrashing();
    }
    return update_service_state;
  }

  static UpdateService::Result QueryResult(ICompleteStatus* complete_status) {
    CHECK(complete_status);

    LONG code = 0;
    base::win::ScopedBstr message;
    CHECK(SUCCEEDED(complete_status->get_statusCode(&code)));

    VLOG(2) << "ICompleteStatus::OnComplete(" << code << ")";
    return static_cast<UpdateService::Result>(code);
  }

  // Called by IUpdaterObserver::OnStateChange when update state changes occur.
  base::RepeatingCallback<void(const UpdateService::UpdateState&)>
      state_update_callback_;

  // Called by IUpdaterObserver::OnComplete when the COM RPC call is done.
  base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
      callback_;

  UpdateService::Result result_ = UpdateService::Result::kSuccess;
};

class UpdaterCallback : public DYNAMICIIDSIMPL(IUpdaterCallback) {
 public:
  explicit UpdaterCallback(
      base::OnceCallback<void(base::expected<LONG, RpcError>)> callback)
      : callback_(std::move(callback)) {}
  explicit UpdaterCallback(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback)
      : callback_(base::BindOnce(
            [](base::OnceCallback<void(base::expected<int, RpcError>)> callback,
               base::expected<LONG, RpcError> result) {
              std::move(callback).Run(
                  result.transform([](LONG x) { return static_cast<int>(x); }));
            },
            std::move(callback))) {}
  UpdaterCallback(const UpdaterCallback&) = delete;
  UpdaterCallback& operator=(const UpdaterCallback&) = delete;

  // Overrides for IUpdaterCallback. Called on a system thread by COM RPC.
  IFACEMETHODIMP Run(LONG status_code) override {
    VLOG(2) << __func__;
    status_code_ = status_code;
    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<void(base::expected<LONG, RpcError>)> Disconnect() {
    VLOG(2) << __func__;
    return std::move(callback_);
  }

 private:
  ~UpdaterCallback() override {
    if (callback_) {
      std::move(callback_).Run(base::ok(status_code_));
    }
  }

  base::OnceCallback<void(base::expected<LONG, RpcError>)> callback_;

  LONG status_code_ = 0;
};

class UpdaterAppStatesCallback
    : public DYNAMICIIDSIMPL(IUpdaterAppStatesCallback) {
 public:
  explicit UpdaterAppStatesCallback(
      base::OnceCallback<
          void(base::expected<std::vector<UpdateService::AppState>, RpcError>)>
          callback)
      : callback_(std::move(callback)) {}
  UpdaterAppStatesCallback(const UpdaterAppStatesCallback&) = delete;
  UpdaterAppStatesCallback& operator=(const UpdaterAppStatesCallback&) = delete;

  // Overrides for IUpdaterAppStatesCallback. Called on a system thread by COM
  // RPC.
  IFACEMETHODIMP Run(VARIANT updater_app_states) override {
    VLOG(2) << __func__;

    if (V_VT(&updater_app_states) != (VT_ARRAY | VT_DISPATCH)) {
      return E_INVALIDARG;
    }

    // The safearray is owned by the caller of `Run`, so ownership is released
    // here after acquiring the `LockScope`.
    base::win::ScopedSafearray safearray(V_ARRAY(&updater_app_states));
    std::optional<base::win::ScopedSafearray::LockScope<VT_DISPATCH>>
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
      constexpr int kMaxTries = 2;
      for (int try_count = 0; try_count < kMaxTries; ++try_count) {
        HResultOr<UpdateService::AppState> service_app_states =
            IUpdaterAppStateToAppState(app_state);
        if (service_app_states.has_value()) {
          app_states_.push_back(*service_app_states);
          break;
        }
        VLOG(2) << "IUpdaterAppStateToAppState returned "
                << service_app_states.error();
        if (try_count == kMaxTries - 1) {
          return service_app_states.error();
        }
      }
    }

    return S_OK;
  }

  // Disconnects this observer from its subject and ensures the callbacks are
  // not posted after this function is called. Returns the completion callback
  // so that the owner of this object can take back the callback ownership.
  base::OnceCallback<
      void(base::expected<std::vector<UpdateService::AppState>, RpcError>)>
  Disconnect() {
    VLOG(2) << __func__;
    return std::move(callback_);
  }

 private:
  ~UpdaterAppStatesCallback() override {
    if (callback_) {
      std::move(callback_).Run(app_states_);
    }
  }

  static HResultOr<UpdateService::AppState> IUpdaterAppStateToAppState(
      Microsoft::WRL::ComPtr<IUpdaterAppState> updater_app_state) {
    CHECK(updater_app_state);
    UpdateService::AppState app_state;
    {
      base::win::ScopedBstr app_id;
      if (HRESULT hr = updater_app_state->get_appId(app_id.Receive());
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      app_state.app_id = base::WideToUTF8(app_id.Get());
    }
    {
      base::win::ScopedBstr version;
      if (HRESULT hr = updater_app_state->get_version(version.Receive());
          HRESULT(hr)) {
        return base::unexpected(hr);
      }
      app_state.version = base::Version(base::WideToUTF8(version.Get()));
    }
    {
      base::win::ScopedBstr ap;
      if (HRESULT hr = updater_app_state->get_ap(ap.Receive()); FAILED(hr)) {
        return base::unexpected(hr);
      }
      app_state.ap = base::WideToUTF8(ap.Get());
    }
    {
      base::win::ScopedBstr brand_code;
      if (HRESULT hr = updater_app_state->get_brandCode(brand_code.Receive());
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      app_state.brand_code = base::WideToUTF8(brand_code.Get());
    }
    {
      base::win::ScopedBstr brand_path;
      if (HRESULT hr = updater_app_state->get_brandPath(brand_path.Receive());
          FAILED(hr)) {
        return base::unexpected(hr);
      }
      app_state.brand_path = base::FilePath(brand_path.Get());
    }
    {
      base::win::ScopedBstr ecp;
      if (HRESULT hr = updater_app_state->get_ecp(ecp.Receive()); FAILED(hr)) {
        return base::unexpected(hr);
      }
      app_state.ecp = base::FilePath(ecp.Get());
    }

    return app_state;
  }

  base::OnceCallback<void(
      base::expected<std::vector<UpdateService::AppState>, RpcError>)>
      callback_;

  std::vector<UpdateService::AppState> app_states_;
};

}  // namespace

class UpdateServiceProxyImplImpl
    : public base::RefCountedThreadSafe<UpdateServiceProxyImplImpl>,
      public ProxyImplBase<UpdateServiceProxyImplImpl,
                           IUpdater,
                           __uuidof(IUpdaterUser),
                           __uuidof(IUpdaterSystem)> {
 public:
  explicit UpdateServiceProxyImplImpl(UpdaterScope scope)
      : ProxyImplBase(scope) {}

  static auto GetClassGuid(UpdaterScope scope) {
    return IsSystemInstall(scope) ? __uuidof(UpdaterSystemClass)
                                  : __uuidof(UpdaterUserClass);
  }

  void GetVersion(
      base::OnceCallback<void(base::expected<base::Version, RpcError>)>
          callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::GetVersionOnTaskRunner,
                       this, std::move(callback)));
  }

  void FetchPolicies(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::FetchPoliciesOnTaskRunner,
                       this, std::move(callback)));
  }

  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::RegisterAppOnTaskRunner,
                       this, request, std::move(callback)));
  }

  void GetAppStates(
      base::OnceCallback<
          void(base::expected<std::vector<UpdateService::AppState>, RpcError>)>
          callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::GetAppStatesOnTaskRunner,
                       this, std::move(callback)));
  }

  void RunPeriodicTasks(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
    PostRPCTask(base::BindOnce(
        &UpdateServiceProxyImplImpl::RunPeriodicTasksOnTaskRunner, this,
        std::move(callback)));
  }

  void CheckForUpdate(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::CheckForUpdateOnTaskRunner,
                       this, app_id, priority, policy_same_version_update,
                       state_update, std::move(callback)));
  }

  void Update(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImplImpl::UpdateOnTaskRunner,
                               this, app_id, install_data_index, priority,
                               policy_same_version_update, state_update,
                               std::move(callback)));
  }

  void UpdateAll(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::UpdateAllOnTaskRunner, this,
                       state_update, std::move(callback)));
  }

  void Install(
      const RegistrationRequest& registration,
      const std::string& client_install_data,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    PostRPCTask(base::BindOnce(&UpdateServiceProxyImplImpl::InstallOnTaskRunner,
                               this, registration, client_install_data,
                               install_data_index, priority, state_update,
                               std::move(callback)));
  }

  void CancelInstalls(const std::string& app_id) {
    PostRPCTask(base::BindOnce(
        &UpdateServiceProxyImplImpl::CancelInstallsOnTaskRunner, this, app_id));
  }

  void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    PostRPCTask(
        base::BindOnce(&UpdateServiceProxyImplImpl::RunInstallerOnTaskRunner,
                       this, app_id, installer_path, install_args, install_data,
                       install_settings, state_update, std::move(callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceProxyImplImpl>;
  virtual ~UpdateServiceProxyImplImpl() = default;

  void GetVersionOnTaskRunner(
      base::OnceCallback<void(base::expected<base::Version, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    base::win::ScopedBstr version;
    if (HRESULT hr = get_interface()->GetVersion(version.Receive());
        FAILED(hr)) {
      VLOG(2) << "IUpdater::GetVersion failed: " << std::hex << hr;
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    std::move(callback).Run(base::Version(base::WideToUTF8(version.Get())));
  }

  void FetchPoliciesOnTaskRunner(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterCallback>(std::move(callback));
    if (HRESULT hr = get_interface()->FetchPolicies(callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::FetchPolicies: " << std::hex << hr;
      callback_wrapper->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void RegisterAppOnTaskRunner(
      const RegistrationRequest& request,
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    std::wstring app_id_w;
    std::wstring brand_code_w;
    std::wstring brand_path_w;
    std::wstring ap_w;
    std::wstring version_w;
    std::wstring existence_checker_path_w;
    if (![&] {
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
      std::move(callback).Run(base::ok(E_INVALIDARG));
      return;
    }

    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterCallback>(std::move(callback));
    if (HRESULT hr = get_interface()->RegisterApp(
            app_id_w.c_str(), brand_code_w.c_str(), brand_path_w.c_str(),
            ap_w.c_str(), version_w.c_str(), existence_checker_path_w.c_str(),
            callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::RegisterApp: " << std::hex << hr;
      callback_wrapper->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void GetAppStatesOnTaskRunner(
      base::OnceCallback<
          void(base::expected<std::vector<UpdateService::AppState>, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterAppStatesCallback>(std::move(callback));
    if (HRESULT hr = get_interface()->GetAppStates(callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::GetAppStates: " << std::hex << hr;
      callback_wrapper->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void RunPeriodicTasksOnTaskRunner(
      base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    auto callback_wrapper =
        MakeComObjectOrCrash<UpdaterCallback>(std::move(callback));
    if (HRESULT hr = get_interface()->RunPeriodicTasks(callback_wrapper.Get());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::RunPeriodicTasks " << std::hex << hr;
      callback_wrapper->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void CheckForUpdateOnTaskRunner(
      const std::string& app_id,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
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
      observer->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void UpdateOnTaskRunner(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      UpdateService::PolicySameVersionUpdate policy_same_version_update,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    std::wstring app_id_w;
    std::wstring install_data_index_w;
    if (![&] {
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
      observer->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void UpdateAllOnTaskRunner(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    auto observer = MakeComObjectOrCrash<UpdaterObserver>(state_update,
                                                          std::move(callback));
    if (HRESULT hr = get_interface()->UpdateAll(observer.Get()); FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::UpdateAll: " << std::hex << hr;
      observer->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void InstallOnTaskRunner(
      const RegistrationRequest& request,
      const std::string& client_install_data,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
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
    if (![&] {
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
      observer->Disconnect().Run(base::unexpected(hr));
      return;
    }
  }

  void CancelInstallsOnTaskRunner(const std::string& app_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (FAILED(ConnectToServer())) {
      return;
    }
    if (HRESULT hr =
            get_interface()->CancelInstalls(base::UTF8ToWide(app_id).c_str());
        FAILED(hr)) {
      VLOG(2) << "Failed to call IUpdater::CancelInstalls: " << std::hex << hr;
    }
  }

  void RunInstallerOnTaskRunner(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
          state_update,
      base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
    if (HRESULT hr = ConnectToServer(); FAILED(hr)) {
      std::move(callback).Run(base::unexpected(hr));
      return;
    }
    std::wstring app_id_w;
    std::wstring install_args_w;
    std::wstring install_data_w;
    std::wstring install_settings_w;
    if (![&] {
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
      observer->Disconnect().Run(base::unexpected(hr));
    }
  }
};

UpdateServiceProxyImpl::UpdateServiceProxyImpl(UpdaterScope updater_scope)
    : impl_(base::MakeRefCounted<UpdateServiceProxyImplImpl>(updater_scope)) {}

UpdateServiceProxyImpl::~UpdateServiceProxyImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  UpdateServiceProxyImplImpl::Destroy(std::move(impl_));
}

void UpdateServiceProxyImpl::GetVersion(
    base::OnceCallback<void(base::expected<base::Version, RpcError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->GetVersion(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::FetchPolicies(
    base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->FetchPolicies(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RegisterApp(request,
                     base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::GetAppStates(
    base::OnceCallback<void(base::expected<std::vector<UpdateService::AppState>,
                                           RpcError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->GetAppStates(base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::RunPeriodicTasks(
    base::OnceCallback<void(base::expected<int, RpcError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RunPeriodicTasks(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::CheckForUpdate(
    const std::string& app_id,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->CheckForUpdate(
      app_id, priority, policy_same_version_update,
      base::BindPostTaskToCurrentDefault(state_update),
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Update(app_id, install_data_index, priority,
                policy_same_version_update,
                base::BindPostTaskToCurrentDefault(state_update),
                base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::UpdateAll(
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->UpdateAll(base::BindPostTaskToCurrentDefault(state_update),
                   base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::Install(
    const RegistrationRequest& registration,
    const std::string& client_install_data,
    const std::string& install_data_index,
    UpdateService::Priority priority,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Install(registration, client_install_data, install_data_index,
                 priority, base::BindPostTaskToCurrentDefault(state_update),
                 base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void UpdateServiceProxyImpl::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->CancelInstalls(app_id);
}

void UpdateServiceProxyImpl::RunInstaller(
    const std::string& app_id,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    const std::string& install_settings,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)>
        state_update,
    base::OnceCallback<void(base::expected<UpdateService::Result, RpcError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->RunInstaller(app_id, installer_path, install_args, install_data,
                      install_settings,
                      base::BindPostTaskToCurrentDefault(state_update),
                      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope,
    base::TimeDelta /*get_version_timeout*/) {
  return base::MakeRefCounted<UpdateServiceProxy>(
      base::MakeRefCounted<UpdateServiceProxyImpl>(updater_scope));
}

}  // namespace updater
