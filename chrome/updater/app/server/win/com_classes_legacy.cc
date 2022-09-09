// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes_legacy.h"

#include <oleauto.h>
#include <shellapi.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/app_command_runner.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

HRESULT OpenCallerProcessHandle(DWORD proc_id,
                                base::win::ScopedHandle& proc_handle) {
  proc_handle.Set(::OpenProcess(PROCESS_DUP_HANDLE, false, proc_id));
  return proc_handle.IsValid() ? S_OK : updater::HRESULTFromLastError();
}

// Extracts a string from a VARIANT if the VARIANT is VT_BSTR or VT_BSTR |
// VT_BYREF. Returns absl::nullopt if the VARIANT is not a BSTR.
absl::optional<std::wstring> StringFromVariant(const VARIANT& source) {
  if (V_VT(&source) == VT_BSTR) {
    return V_BSTR(&source);
  }

  if (V_VT(&source) == (VT_BSTR | VT_BYREF)) {
    return *(V_BSTRREF(&source));
  }

  return {};
}

template <typename T>
std::string GetStringFromValue(const T& value) {
  return value;
}

template <>
std::string GetStringFromValue(const int& value) {
  return base::NumberToString(value);
}

template <>
std::string GetStringFromValue(const bool& value) {
  return value ? "true" : "false";
}

template <>
std::string GetStringFromValue(const updater::UpdatesSuppressedTimes& value) {
  return base::StringPrintf("%d, %d, %d", value.start_hour_,
                            value.start_minute_, value.duration_minute_);
}

template <>
std::string GetStringFromValue(const std::vector<std::string>& value) {
  return base::JoinString(value, ";");
}

}  // namespace

namespace updater {

LegacyOnDemandImpl::LegacyOnDemandImpl()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

LegacyOnDemandImpl::~LegacyOnDemandImpl() = default;

STDMETHODIMP LegacyOnDemandImpl::createAppBundleWeb(
    IDispatch** app_bundle_web) {
  DCHECK(app_bundle_web);
  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle(this);
  *app_bundle_web = app_bundle.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::createApp(BSTR app_id,
                                           BSTR brand_code,
                                           BSTR language,
                                           BSTR ap) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::createInstalledApp(BSTR app_id) {
  set_app_id(base::WideToASCII(app_id));
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::createAllInstalledApps() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_displayLanguage(BSTR* language) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::put_displayLanguage(BSTR language) {
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::put_parentHWND(ULONG_PTR hwnd) {
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_length(int* number) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_appWeb(int index, IDispatch** app_web) {
  DCHECK_EQ(index, 0);
  DCHECK(app_web);

  Microsoft::WRL::ComPtr<IAppWeb> app(this);
  *app_web = app.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::initialize() {
  return S_OK;
}

// Invokes the in-process update service on the main sequence. Forwards the
// callbacks to a sequenced task runner. |obj| is bound to this object.
STDMETHODIMP LegacyOnDemandImpl::checkForUpdate() {
  using LegacyOnDemandImplPtr = Microsoft::WRL::ComPtr<LegacyOnDemandImpl>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             LegacyOnDemandImplPtr obj) {
            update_service->Update(
                obj->app_id(), "", UpdateService::Priority::kForeground,
                UpdateService::PolicySameVersionUpdate::kNotAllowed,
                base::BindRepeating(
                    [](LegacyOnDemandImplPtr obj,
                       const UpdateService::UpdateState& state_update) {
                      obj->task_runner_->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &LegacyOnDemandImpl::UpdateStateCallback, obj,
                              state_update));
                    },
                    obj),
                base::BindOnce(
                    [](LegacyOnDemandImplPtr obj,
                       UpdateService::Result result) {
                      obj->task_runner_->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &LegacyOnDemandImpl::UpdateResultCallback, obj,
                              result));
                    },
                    obj));
          },
          com_server->update_service(), LegacyOnDemandImplPtr(this)));
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::download() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::install() {
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::pause() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::resume() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::cancel() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::downloadPackage(BSTR app_id,
                                                 BSTR package_name) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_currentState(VARIANT* current_state) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_appId(BSTR* app_id) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_currentVersionWeb(IDispatch** current) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_nextVersionWeb(IDispatch** next) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_command(BSTR command_id,
                                             IDispatch** command) {
  return Microsoft::WRL::MakeAndInitialize<LegacyAppCommandWebImpl>(
      command, GetUpdaterScope(), base::UTF8ToWide(app_id()), command_id);
}

STDMETHODIMP LegacyOnDemandImpl::get_currentState(IDispatch** current_state) {
  DCHECK(current_state);
  Microsoft::WRL::ComPtr<ICurrentState> state(this);
  *current_state = state.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::launch() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::uninstall() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_serverInstallDataIndex(BSTR* language) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::put_serverInstallDataIndex(BSTR language) {
  return E_NOTIMPL;
}

// Returns the state of update as seen by the on-demand client:
// - if the repeading callback has been received: returns the specific state.
// - if the completion callback has been received, but no repeating callback,
//   then it returns STATE_ERROR. This is an error state and it indicates that
//   update is not going to be further handled and repeating callbacks posted.
// - if no callback has been received at all: returns STATE_INIT.
STDMETHODIMP LegacyOnDemandImpl::get_stateValue(LONG* state_value) {
  DCHECK(state_value);
  base::AutoLock lock{lock_};
  if (state_update_) {
    switch (state_update_.value().state) {
      case UpdateService::UpdateState::State::kUnknown:  // Fall through.
      case UpdateService::UpdateState::State::kNotStarted:
        *state_value = STATE_INIT;
        break;
      case UpdateService::UpdateState::State::kCheckingForUpdates:
        *state_value = STATE_CHECKING_FOR_UPDATE;
        break;
      case UpdateService::UpdateState::State::kUpdateAvailable:
        *state_value = STATE_UPDATE_AVAILABLE;
        break;
      case UpdateService::UpdateState::State::kDownloading:
        *state_value = STATE_DOWNLOADING;
        break;
      case UpdateService::UpdateState::State::kInstalling:
        *state_value = STATE_INSTALLING;
        break;
      case UpdateService::UpdateState::State::kUpdated:
        *state_value = STATE_INSTALL_COMPLETE;
        break;
      case UpdateService::UpdateState::State::kNoUpdate:
        *state_value = STATE_NO_UPDATE;
        break;
      case UpdateService::UpdateState::State::kUpdateError:
        *state_value = STATE_ERROR;
        break;
    }
  } else if (result_) {
    DCHECK_NE(result_.value(), UpdateService::Result::kSuccess);
    *state_value = STATE_ERROR;
  } else {
    *state_value = STATE_INIT;
  }

  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_availableVersion(BSTR* available_version) {
  base::AutoLock lock{lock_};
  if (state_update_) {
    *available_version =
        base::win::ScopedBstr(
            base::UTF8ToWide(state_update_->next_version.GetString()))
            .Release();
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_bytesDownloaded(ULONG* bytes_downloaded) {
  DCHECK(bytes_downloaded);
  base::AutoLock lock{lock_};
  if (!state_update_ || state_update_->downloaded_bytes == -1)
    return E_FAIL;
  *bytes_downloaded = state_update_->downloaded_bytes;
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_totalBytesToDownload(
    ULONG* total_bytes_to_download) {
  DCHECK(total_bytes_to_download);
  base::AutoLock lock{lock_};
  if (!state_update_ || state_update_->total_bytes == -1)
    return E_FAIL;
  *total_bytes_to_download = state_update_->total_bytes;
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_downloadTimeRemainingMs(
    LONG* download_time_remaining_ms) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_nextRetryTime(ULONGLONG* next_retry_time) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_installProgress(
    LONG* install_progress_percentage) {
  DCHECK(install_progress_percentage);
  base::AutoLock lock{lock_};
  if (!state_update_ || state_update_->install_progress == -1)
    return E_FAIL;
  *install_progress_percentage = state_update_->install_progress;
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_installTimeRemainingMs(
    LONG* install_time_remaining_ms) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_isCanceled(VARIANT_BOOL* is_canceled) {
  return E_NOTIMPL;
}

// In the error case, if an installer error occurred, it remaps the installer
// error to the legacy installer error value, for backward compatibility.
STDMETHODIMP LegacyOnDemandImpl::get_errorCode(LONG* error_code) {
  DCHECK(error_code);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->state == UpdateService::UpdateState::State::kUpdateError) {
    *error_code = state_update_->error_code == kErrorApplicationInstallerFailed
                      ? GOOPDATEINSTALL_E_INSTALLER_FAILED
                      : state_update_->error_code;
  } else if (result_) {
    *error_code = (result_.value() == UpdateService::Result::kSuccess) ? 0 : -1;
  } else {
    *error_code = 0;
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_extraCode1(LONG* extra_code1) {
  DCHECK(extra_code1);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->state == UpdateService::UpdateState::State::kUpdateError) {
    *extra_code1 = state_update_->extra_code1;
  } else {
    *extra_code1 = 0;
  }
  return S_OK;
}

// Returns an installer error completion message.
STDMETHODIMP LegacyOnDemandImpl::get_completionMessage(
    BSTR* completion_message) {
  DCHECK(completion_message);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->error_code == kErrorApplicationInstallerFailed) {
    // TODO(1095133): this string needs localization.
    *completion_message = base::win::ScopedBstr(L"Installer failed.").Release();
  } else {
    completion_message = nullptr;
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_installerResultCode(
    LONG* installer_result_code) {
  DCHECK(installer_result_code);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->error_code == kErrorApplicationInstallerFailed) {
    *installer_result_code = state_update_->extra_code1;
  } else {
    *installer_result_code = 0;
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_installerResultExtraCode1(
    LONG* installer_result_extra_code1) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_postInstallLaunchCommandLine(
    BSTR* post_install_launch_command_line) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_postInstallUrl(BSTR* post_install_url) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_postInstallAction(
    LONG* post_install_action) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetTypeInfoCount(UINT*) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetTypeInfo(UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetIDsOfNames(REFIID,
                                               LPOLESTR*,
                                               UINT,
                                               LCID,
                                               DISPID*) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::Invoke(DISPID,
                                        REFIID,
                                        LCID,
                                        WORD,
                                        DISPPARAMS*,
                                        VARIANT*,
                                        EXCEPINFO*,
                                        UINT*) {
  return E_NOTIMPL;
}

void LegacyOnDemandImpl::UpdateStateCallback(
    UpdateService::UpdateState state_update) {
  base::AutoLock lock{lock_};
  state_update_ = state_update;
}

void LegacyOnDemandImpl::UpdateResultCallback(UpdateService::Result result) {
  base::AutoLock lock{lock_};
  result_ = result;
}

LegacyProcessLauncherImpl::LegacyProcessLauncherImpl() = default;
LegacyProcessLauncherImpl::~LegacyProcessLauncherImpl() = default;

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdLine(const WCHAR* cmd_line) {
  return LaunchCmdLineEx(cmd_line, nullptr, nullptr, nullptr);
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchBrowser(DWORD browser_type,
                                                      const WCHAR* url) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdElevated(
    const WCHAR* app_id,
    const WCHAR* command_id,
    DWORD caller_proc_id,
    ULONG_PTR* proc_handle) {
  AppCommandRunner app_command_runner;
  if (HRESULT hr = AppCommandRunner::LoadAppCommand(
          UpdaterScope::kSystem, app_id, command_id, app_command_runner);
      FAILED(hr)) {
    return hr;
  }

  base::win::ScopedHandle caller_proc_handle;
  if (HRESULT hr = OpenCallerProcessHandle(caller_proc_id, caller_proc_handle);
      FAILED(hr)) {
    VLOG(1) << "failed to open caller's handle " << hr;
    return hr;
  }

  base::Process process;
  if (HRESULT hr = app_command_runner.Run({}, process); FAILED(hr)) {
    return hr;
  }

  ScopedKernelHANDLE duplicate_proc_handle;
  if (!::DuplicateHandle(
          ::GetCurrentProcess(), process.Handle(), caller_proc_handle.Get(),
          ScopedKernelHANDLE::Receiver(duplicate_proc_handle).get(),
          PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, 0)) {
    HRESULT hr = HRESULTFromLastError();
    VLOG(1) << "Failed to duplicate the handle " << hr;
    return hr;
  }

  // The caller must close this handle.
  *proc_handle = reinterpret_cast<ULONG_PTR>(duplicate_proc_handle.release());
  return S_OK;
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdLineEx(
    const WCHAR* cmd_line,
    DWORD* server_proc_id,
    ULONG_PTR* proc_handle,
    ULONG_PTR* stdout_handle) {
  return E_NOTIMPL;
}

LegacyAppCommandWebImpl::LegacyAppCommandWebImpl() = default;
LegacyAppCommandWebImpl::~LegacyAppCommandWebImpl() = default;

HRESULT LegacyAppCommandWebImpl::RuntimeClassInitialize(
    UpdaterScope scope,
    const std::wstring& app_id,
    const std::wstring& command_id) {
  if (HRESULT hr = AppCommandRunner::LoadAppCommand(scope, app_id, command_id,
                                                    app_command_runner_);
      FAILED(hr)) {
    return hr;
  }

  return InitializeTypeInfo();
}

STDMETHODIMP LegacyAppCommandWebImpl::get_status(UINT* status) {
  DCHECK(status);

  if (!process_.IsValid()) {
    *status = COMMAND_STATUS_INIT;
  } else if (process_.IsRunning()) {
    *status = COMMAND_STATUS_RUNNING;
  } else {
    *status = COMMAND_STATUS_COMPLETE;
  }

  return S_OK;
}

STDMETHODIMP LegacyAppCommandWebImpl::get_exitCode(DWORD* exit_code) {
  DCHECK(exit_code);

  int code = -1;
  if (!process_.IsValid() ||
      !process_.WaitForExitWithTimeout(base::TimeDelta(), &code)) {
    return E_FAIL;
  }

  *exit_code = code;
  return S_OK;
}

STDMETHODIMP LegacyAppCommandWebImpl::get_output(BSTR* output) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyAppCommandWebImpl::execute(VARIANT substitution1,
                                              VARIANT substitution2,
                                              VARIANT substitution3,
                                              VARIANT substitution4,
                                              VARIANT substitution5,
                                              VARIANT substitution6,
                                              VARIANT substitution7,
                                              VARIANT substitution8,
                                              VARIANT substitution9) {
  std::vector<std::wstring> substitutions;
  for (const VARIANT& substitution :
       {substitution1, substitution2, substitution3, substitution4,
        substitution5, substitution6, substitution7, substitution8,
        substitution9}) {
    const absl::optional<std::wstring> substitution_string =
        StringFromVariant(substitution);
    if (!substitution_string)
      break;

    VLOG(2) << __func__
            << " substitution_string: " << substitution_string.value();
    substitutions.push_back(substitution_string.value());
  }

  return app_command_runner_.Run(substitutions, process_);
}

STDMETHODIMP LegacyAppCommandWebImpl::GetTypeInfoCount(UINT* type_info_count) {
  *type_info_count = 1;
  return S_OK;
}

STDMETHODIMP LegacyAppCommandWebImpl::GetTypeInfo(UINT type_info_index,
                                                  LCID locale_id,
                                                  ITypeInfo** type_info) {
  if (type_info_index != 0)
    return E_INVALIDARG;

  return type_info_.CopyTo(type_info);
}

STDMETHODIMP LegacyAppCommandWebImpl::GetIDsOfNames(
    REFIID iid,
    LPOLESTR* names_to_be_mapped,
    UINT count_of_names_to_be_mapped,
    LCID locale_id,
    DISPID* dispatch_ids) {
  return type_info_->GetIDsOfNames(names_to_be_mapped,
                                   count_of_names_to_be_mapped, dispatch_ids);
}

STDMETHODIMP LegacyAppCommandWebImpl::Invoke(DISPID dispatch_id,
                                             REFIID iid,
                                             LCID locale_id,
                                             WORD flags,
                                             DISPPARAMS* dispatch_parameters,
                                             VARIANT* result,
                                             EXCEPINFO* exception_info,
                                             UINT* arg_error_index) {
  HRESULT hr = type_info_->Invoke(
      Microsoft::WRL::ComPtr<IAppCommandWeb>(this).Get(), dispatch_id, flags,
      dispatch_parameters, result, exception_info, arg_error_index);
  if (FAILED(hr)) {
    LOG(ERROR) << __func__ << " type_info_->Invoke failed: " << dispatch_id
               << ": " << std::hex << hr;
  }

  return hr;
}

HRESULT LegacyAppCommandWebImpl::InitializeTypeInfo() {
  base::FilePath typelib_path;
  if (!base::PathService::Get(base::DIR_EXE, &typelib_path))
    return E_UNEXPECTED;

  typelib_path =
      typelib_path.Append(GetExecutableRelativePath())
          .Append(GetComTypeLibResourceIndex(__uuidof(IAppCommandWeb)));

  Microsoft::WRL::ComPtr<ITypeLib> type_lib;
  if (HRESULT hr = ::LoadTypeLib(typelib_path.value().c_str(), &type_lib);
      FAILED(hr)) {
    LOG(ERROR) << __func__ << " ::LoadTypeLib failed: " << typelib_path << ": "
               << std::hex << hr;
    return hr;
  }

  if (HRESULT hr =
          type_lib->GetTypeInfoOfGuid(__uuidof(IAppCommandWeb), &type_info_);
      FAILED(hr)) {
    LOG(ERROR) << __func__ << " ::GetTypeInfoOfGuid failed"
               << ": " << std::hex << hr << ": IID_IAppCommand: "
               << base::win::WStringFromGUID(__uuidof(IAppCommandWeb));
    return hr;
  }

  return S_OK;
}

PolicyStatusImpl::PolicyStatusImpl()
    : policy_service_(
          AppServerSingletonInstance()->config()->GetPolicyService()) {}
PolicyStatusImpl::~PolicyStatusImpl() = default;

HRESULT PolicyStatusImpl::RuntimeClassInitialize() {
  return S_OK;
}

// IPolicyStatus.
STDMETHODIMP PolicyStatusImpl::get_lastCheckPeriodMinutes(DWORD* minutes) {
  DCHECK(minutes);

  int period = 0;
  if (!policy_service_->GetLastCheckPeriodMinutes(nullptr, &period))
    return E_FAIL;

  *minutes = period;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_updatesSuppressedTimes(
    DWORD* start_hour,
    DWORD* start_min,
    DWORD* duration_min,
    VARIANT_BOOL* are_updates_suppressed) {
  DCHECK(start_hour);
  DCHECK(start_min);
  DCHECK(duration_min);
  DCHECK(are_updates_suppressed);

  UpdatesSuppressedTimes updates_suppressed_times;
  if (!policy_service_->GetUpdatesSuppressedTimes(nullptr,
                                                  &updates_suppressed_times) ||
      !updates_suppressed_times.valid()) {
    return E_FAIL;
  }

  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  *start_hour = updates_suppressed_times.start_hour_;
  *start_min = updates_suppressed_times.start_minute_;
  *duration_min = updates_suppressed_times.duration_minute_;
  *are_updates_suppressed =
      updates_suppressed_times.contains(now.hour, now.minute) ? VARIANT_TRUE
                                                              : VARIANT_FALSE;

  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_downloadPreferenceGroupPolicy(BSTR* pref) {
  DCHECK(pref);

  std::string download_preference;
  if (!policy_service_->GetDownloadPreferenceGroupPolicy(
          nullptr, &download_preference)) {
    return E_FAIL;
  }

  *pref =
      base::win::ScopedBstr(base::ASCIIToWide(download_preference)).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheSizeLimitMBytes(DWORD* limit) {
  DCHECK(limit);

  int cache_size_limit = 0;
  if (!policy_service_->GetPackageCacheSizeLimitMBytes(nullptr,
                                                       &cache_size_limit)) {
    return E_FAIL;
  }

  *limit = cache_size_limit;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheExpirationTimeDays(DWORD* days) {
  DCHECK(days);

  int cache_life_limit = 0;
  if (!policy_service_->GetPackageCacheExpirationTimeDays(nullptr,
                                                          &cache_life_limit)) {
    return E_FAIL;
  }

  *days = cache_life_limit;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppInstalls(
    BSTR app_id,
    DWORD* policy) {
  DCHECK(policy);

  int install_policy = 0;
  if (!policy_service_->GetEffectivePolicyForAppInstalls(
          base::WideToASCII(app_id), nullptr, &install_policy)) {
    return E_FAIL;
  }

  *policy = install_policy;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppUpdates(BSTR app_id,
                                                                DWORD* policy) {
  DCHECK(policy);

  int update_policy = 0;
  if (!policy_service_->GetEffectivePolicyForAppUpdates(
          base::WideToASCII(app_id), nullptr, &update_policy)) {
    return E_FAIL;
  }

  *policy = update_policy;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_targetVersionPrefix(BSTR app_id,
                                                       BSTR* prefix) {
  DCHECK(prefix);

  std::string target_version_prefix;
  if (!policy_service_->GetTargetVersionPrefix(
          base::WideToASCII(app_id), nullptr, &target_version_prefix)) {
    return E_FAIL;
  }

  *prefix =
      base::win::ScopedBstr(base::ASCIIToWide(target_version_prefix)).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_isRollbackToTargetVersionAllowed(
    BSTR app_id,
    VARIANT_BOOL* rollback_allowed) {
  DCHECK(rollback_allowed);

  bool is_rollback_allowed = false;
  if (!policy_service_->IsRollbackToTargetVersionAllowed(
          base::WideToASCII(app_id), nullptr, &is_rollback_allowed)) {
    return E_FAIL;
  }

  *rollback_allowed = is_rollback_allowed ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_updaterVersion(BSTR* version) {
  DCHECK(version);

  *version = base::win::ScopedBstr(kUpdaterVersionUtf16).Release();
  return S_OK;
}

namespace {

// Holds the result of the IPC to retrieve `last checked time`.
struct LastCheckedTimeResult
    : public base::RefCountedThreadSafe<LastCheckedTimeResult> {
  absl::optional<DATE> last_checked_time;
  base::WaitableEvent completion_event;

 private:
  friend class base::RefCountedThreadSafe<LastCheckedTimeResult>;
  virtual ~LastCheckedTimeResult() = default;
};

// Holds the result of the IPC to retrieve PolicyService data.
template <typename T>
class PolicyStatusResult
    : public base::RefCountedThreadSafe<PolicyStatusResult<T>> {
 public:
  using ValueGetter = base::RepeatingCallback<bool(PolicyStatus<T>*, T*)>;

  static auto Get(ValueGetter value_getter) {
    auto result = base::WrapRefCounted(new PolicyStatusResult<T>(value_getter));
    AppServerSingletonInstance()->main_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PolicyStatusResult::GetValueOnSequence, result));
    result->completion_event.TimedWait(base::Seconds(60));
    return result->value;
  }

 private:
  friend base::RefCountedThreadSafe<PolicyStatusResult<T>>;
  virtual ~PolicyStatusResult() = default;

  explicit PolicyStatusResult(ValueGetter value_getter)
      : value_getter(value_getter) {}

  void GetValueOnSequence() {
    PolicyStatus<T> policy_status;
    if (value_getter.Run(&policy_status, nullptr)) {
      value = policy_status;
    }
    completion_event.Signal();
  }

  ValueGetter value_getter;
  absl::optional<PolicyStatus<T>> value;
  base::WaitableEvent completion_event;
};

}  // namespace

STDMETHODIMP PolicyStatusImpl::get_lastCheckedTime(DATE* last_checked) {
  DCHECK(last_checked);

  using PolicyStatusImplPtr = Microsoft::WRL::ComPtr<PolicyStatusImpl>;
  auto result = base::MakeRefCounted<LastCheckedTimeResult>();
  AppServerSingletonInstance()->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](PolicyStatusImplPtr obj,
             scoped_refptr<LastCheckedTimeResult> result) {
            const base::ScopedClosureRunner signal_event(base::BindOnce(
                [](scoped_refptr<LastCheckedTimeResult> result) {
                  result->completion_event.Signal();
                },
                result));

            const base::Time last_checked_time =
                base::MakeRefCounted<const PersistedData>(
                    AppServerSingletonInstance()->prefs()->GetPrefService())
                    ->GetLastChecked();
            if (last_checked_time.is_null())
              return;

            const FILETIME last_checked_filetime =
                last_checked_time.ToFileTime();
            FILETIME file_time_local = {};
            SYSTEMTIME system_time = {};
            DATE last_checked_variant_time = {};
            if (::FileTimeToLocalFileTime(&last_checked_filetime,
                                          &file_time_local) &&
                ::FileTimeToSystemTime(&file_time_local, &system_time) &&
                ::SystemTimeToVariantTime(&system_time,
                                          &last_checked_variant_time)) {
              result->last_checked_time = last_checked_variant_time;
            }
          },
          PolicyStatusImplPtr(this), result));

  if (!result->completion_event.TimedWait(base::Seconds(60)) ||
      !result->last_checked_time.has_value()) {
    return E_FAIL;
  }

  *last_checked = *result->last_checked_time;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::refreshPolicies() {
  // Capture `this` object throughout the policy fetch to have an outstanding
  // self reference of the COM object, otherwise the server could shutdown if
  // the caller releases its interface pointer when this function returns.
  using PolicyStatusImplPtr = Microsoft::WRL::ComPtr<PolicyStatusImpl>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UpdateService::FetchPolicies, com_server->update_service(),
          base::BindOnce([](PolicyStatusImplPtr /* obj */, int /* result */) {},
                         PolicyStatusImplPtr(this))));
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_lastCheckPeriodMinutes(
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(base::BindRepeating(
      &PolicyService::GetLastCheckPeriodMinutes, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_updatesSuppressedTimes(
    IPolicyStatusValue** value,
    VARIANT_BOOL* are_updates_suppressed) {
  DCHECK(value);
  DCHECK(are_updates_suppressed);

  auto policy_status =
      PolicyStatusResult<UpdatesSuppressedTimes>::Get(base::BindRepeating(
          &PolicyService::GetUpdatesSuppressedTimes, policy_service_));
  if (!policy_status.has_value())
    return E_FAIL;
  const UpdatesSuppressedTimes updates_suppressed_times =
      policy_status->effective_policy()->policy;
  if (!updates_suppressed_times.valid())
    return E_FAIL;
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  *are_updates_suppressed =
      updates_suppressed_times.contains(now.hour, now.minute) ? VARIANT_TRUE
                                                              : VARIANT_FALSE;
  return PolicyStatusValueImpl::Create(*policy_status, value);
}

STDMETHODIMP PolicyStatusImpl::get_downloadPreferenceGroupPolicy(
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(base::BindRepeating(
      &PolicyService::GetDownloadPreferenceGroupPolicy, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheSizeLimitMBytes(
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(base::BindRepeating(
      &PolicyService::GetPackageCacheSizeLimitMBytes, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheExpirationTimeDays(
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(base::BindRepeating(
      &PolicyService::GetPackageCacheExpirationTimeDays, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_proxyMode(IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetProxyMode, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_proxyPacUrl(IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetProxyPacUrl, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_proxyServer(IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetProxyServer, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppInstalls(
    BSTR app_id,
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(
      base::BindRepeating(&PolicyService::GetEffectivePolicyForAppInstalls,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppUpdates(
    BSTR app_id,
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(
      base::BindRepeating(&PolicyService::GetEffectivePolicyForAppUpdates,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_targetVersionPrefix(
    BSTR app_id,
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetTargetVersionPrefix,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_isRollbackToTargetVersionAllowed(
    BSTR app_id,
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<bool>::Get(
      base::BindRepeating(&PolicyService::IsRollbackToTargetVersionAllowed,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_targetChannel(BSTR app_id,
                                                 IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetTargetChannel, policy_service_,
                          base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_forceInstallApps(
    VARIANT_BOOL is_machine,
    IPolicyStatusValue** value) {
  DCHECK(value);
  auto policy_status =
      PolicyStatusResult<std::vector<std::string>>::Get(base::BindRepeating(
          &PolicyService::GetForceInstallApps, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

// TODO(crbug.com/1344200): Implement the IDispatch methods.
STDMETHODIMP PolicyStatusImpl::GetTypeInfoCount(UINT*) {
  return E_NOTIMPL;
}

STDMETHODIMP PolicyStatusImpl::GetTypeInfo(UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}

STDMETHODIMP PolicyStatusImpl::GetIDsOfNames(REFIID,
                                             LPOLESTR*,
                                             UINT,
                                             LCID,
                                             DISPID*) {
  return E_NOTIMPL;
}

STDMETHODIMP PolicyStatusImpl::Invoke(DISPID,
                                      REFIID,
                                      LCID,
                                      WORD,
                                      DISPPARAMS*,
                                      VARIANT*,
                                      EXCEPINFO*,
                                      UINT*) {
  return E_NOTIMPL;
}

PolicyStatusValueImpl::PolicyStatusValueImpl() = default;
PolicyStatusValueImpl::~PolicyStatusValueImpl() = default;

template <typename T>
HRESULT PolicyStatusValueImpl::Create(
    const T& value,
    IPolicyStatusValue** policy_status_value) {
  return Microsoft::WRL::MakeAndInitialize<PolicyStatusValueImpl>(
      policy_status_value,
      value.effective_policy() ? value.effective_policy()->source : "",
      value.effective_policy()
          ? GetStringFromValue(value.effective_policy()->policy)
          : "",
      value.conflict_policy() != absl::nullopt,
      value.conflict_policy() ? value.conflict_policy()->source : "",
      value.conflict_policy()
          ? GetStringFromValue(value.conflict_policy()->policy)
          : "");
}

HRESULT PolicyStatusValueImpl::RuntimeClassInitialize(
    const std::string& source,
    const std::string& value,
    bool has_conflict,
    const std::string& conflict_source,
    const std::string& conflict_value) {
  source_ = base::ASCIIToWide(source);
  value_ = base::ASCIIToWide(value);
  has_conflict_ = has_conflict ? VARIANT_TRUE : VARIANT_FALSE;
  conflict_source_ = base::ASCIIToWide(conflict_source);
  conflict_value_ = base::ASCIIToWide(conflict_value);

  return S_OK;
}

// IPolicyStatusValue.
STDMETHODIMP PolicyStatusValueImpl::get_source(BSTR* source) {
  DCHECK(source);

  *source = base::win::ScopedBstr(source_).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_value(BSTR* value) {
  DCHECK(value);

  *value = base::win::ScopedBstr(value_).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_hasConflict(
    VARIANT_BOOL* has_conflict) {
  DCHECK(has_conflict);

  *has_conflict = has_conflict_;
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_conflictSource(BSTR* conflict_source) {
  DCHECK(conflict_source);

  *conflict_source = base::win::ScopedBstr(conflict_source_).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_conflictValue(BSTR* conflict_value) {
  DCHECK(conflict_value);

  *conflict_value = base::win::ScopedBstr(conflict_value_).Release();
  return S_OK;
}

// TODO(crbug.com/1344200): Implement the IDispatch methods.
STDMETHODIMP PolicyStatusValueImpl::GetTypeInfoCount(UINT*) {
  return E_NOTIMPL;
}

STDMETHODIMP PolicyStatusValueImpl::GetTypeInfo(UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}

STDMETHODIMP PolicyStatusValueImpl::GetIDsOfNames(REFIID,
                                                  LPOLESTR*,
                                                  UINT,
                                                  LCID,
                                                  DISPID*) {
  return E_NOTIMPL;
}

STDMETHODIMP PolicyStatusValueImpl::Invoke(DISPID,
                                           REFIID,
                                           LCID,
                                           WORD,
                                           DISPPARAMS*,
                                           VARIANT*,
                                           EXCEPINFO*,
                                           UINT*) {
  return E_NOTIMPL;
}

}  // namespace updater
