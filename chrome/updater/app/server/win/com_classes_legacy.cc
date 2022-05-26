// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

HRESULT OpenCallerProcessHandle(DWORD proc_id,
                                base::win::ScopedHandle& proc_handle) {
  proc_handle.Set(::OpenProcess(PROCESS_DUP_HANDLE, false, proc_id));
  return proc_handle.IsValid() ? S_OK : updater::HRESULTFromLastError();
}

std::wstring GetCommandToLaunch(const WCHAR* app_guid, const WCHAR* cmd_id) {
  if (!app_guid || !cmd_id)
    return std::wstring();

  base::win::RegKey key(HKEY_LOCAL_MACHINE, CLIENTS_KEY,
                        updater::Wow6432(KEY_READ));
  if (key.OpenKey(app_guid, updater::Wow6432(KEY_READ)) != ERROR_SUCCESS)
    return std::wstring();

  std::wstring cmd_line;
  key.ReadValue(cmd_id, &cmd_line);
  return cmd_line;
}

HRESULT LaunchCmd(const std::wstring& cmd,
                  const base::win::ScopedHandle& caller_proc_handle,
                  ULONG_PTR* proc_handle) {
  if (cmd.empty() || !caller_proc_handle.IsValid() || !proc_handle)
    return E_INVALIDARG;

  *proc_handle = NULL;

  STARTUPINFOW startup_info = {sizeof(startup_info)};
  PROCESS_INFORMATION process_information = {0};
  std::wstring cmd_line(cmd);
  if (!::CreateProcess(nullptr, &cmd_line[0], nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup_info,
                       &process_information)) {
    return updater::HRESULTFromLastError();
  }

  base::win::ScopedProcessInformation pi(process_information);
  DCHECK(pi.IsValid());

  HANDLE duplicate_proc_handle = NULL;

  bool res = ::DuplicateHandle(
                 ::GetCurrentProcess(),     // Current process.
                 pi.process_handle(),       // Process handle to duplicate.
                 caller_proc_handle.Get(),  // Process receiving the handle.
                 &duplicate_proc_handle,    // Duplicated handle.
                 PROCESS_QUERY_INFORMATION |
                     SYNCHRONIZE,  // Access requested for the new handle.
                 FALSE,            // Don't inherit the new handle.
                 0) != 0;          // Flags.
  if (!res) {
    HRESULT hr = updater::HRESULTFromLastError();
    VLOG(1) << "Failed to duplicate the handle " << hr;
    return hr;
  }

  // The caller must close this handle.
  *proc_handle = reinterpret_cast<ULONG_PTR>(duplicate_proc_handle);
  return S_OK;
}

// Extracts a string from a VARIANT if the VARIANT is VT_BSTR or VT_BSTR |
// VT_BYREF. Returns an empty string if the VARIANT is not a BSTR.
std::wstring StringFromVariant(const VARIANT& source) {
  if (V_VT(&source) == VT_BSTR) {
    return V_BSTR(&source);
  }

  if (V_VT(&source) == (VT_BSTR | VT_BYREF)) {
    return *(V_BSTRREF(&source));
  }

  return {};
}

// Formats a single `parameter` and returns the result. Any placeholder `%N` in
// `parameter` is replaced with substitutions[N - 1]. Any literal `%` needs to
// be escaped with a `%`.
//
// Returns `absl::nullopt` if:
// * a placeholder %N is encountered where N > substitutions.size().
// * a literal `%` is not escaped with a `%`.
//
// See examples in the LegacyAppCommandWebImplTest.FormatParameters* unit tests.
absl::optional<std::wstring> FormatParameter(
    const std::vector<std::wstring>& substitutions,
    const std::wstring& parameter) {
  DCHECK_LE(substitutions.size(), 9U);

  std::wstring formatted_parameter;
  for (auto i = parameter.begin(); i != parameter.end(); ++i) {
    if (*i != '%') {
      formatted_parameter.push_back(*i);
      continue;
    }

    if (++i == parameter.end())
      return absl::nullopt;

    if (*i == '%') {
      formatted_parameter.push_back('%');
      continue;
    }

    if (*i < '1' || *i > '9')
      return absl::nullopt;

    const size_t index = *i - '1';
    if (index >= substitutions.size())
      return absl::nullopt;

    formatted_parameter.append(substitutions[index]);
  }

  return formatted_parameter;
}

// Quotes `input` if necessary so that it will be interpreted as a single
// command-line parameter according to the rules for ::CommandLineToArgvW.
//
// ::CommandLineToArgvW has a special interpretation of backslash characters
// when they are followed by a quotation mark character ("). This interpretation
// assumes that any preceding argument is a valid file system path, or else it
// may behave unpredictably.
//
// This special interpretation controls the "in quotes" mode tracked by the
// parser. When this mode is off, whitespace terminates the current argument.
// When on, whitespace is added to the argument like all other characters.

// * 2n backslashes followed by a quotation mark produce n backslashes followed
// by begin/end quote. This does not become part of the parsed argument, but
// toggles the "in quotes" mode.
// * (2n) + 1 backslashes followed by a quotation mark again produce n
// backslashes followed by a quotation mark literal ("). This does not toggle
// the "in quotes" mode.
// * n backslashes not followed by a quotation mark simply produce n
// backslashes.
//
// See examples in the LegacyAppCommandWebImplTest.ParameterQuoting unit test.
std::wstring QuoteForCommandLineToArgvW(const std::wstring& input) {
  if (input.empty())
    return L"\"\"";

  std::wstring output;
  const bool contains_whitespace =
      input.find_first_of(L" \t") != std::wstring::npos;
  if (contains_whitespace)
    output.push_back(L'"');

  size_t slash_count = 0;
  for (auto i = input.begin(); i != input.end(); ++i) {
    if (*i == L'"') {
      // Before a quote, output 2n backslashes.
      while (slash_count > 0) {
        output.append(L"\\\\");
        --slash_count;
      }
      output.append(L"\\\"");
    } else if (*i != L'\\' || i + 1 == input.end()) {
      // At the end of the string, or before a regular character, output queued
      // slashes.
      while (slash_count > 0) {
        output.push_back(L'\\');
        --slash_count;
      }
      // If this is a slash, it's also the last character. Otherwise, it is just
      // a regular non-quote/non-slash character.
      output.push_back(*i);
    } else if (*i == L'\\') {
      // This is a slash, possibly followed by a quote, not the last character.
      // Queue it up and output it later.
      ++slash_count;
    }
  }

  if (contains_whitespace)
    output.push_back(L'"');

  return output;
}

bool IsParentOf(int key, const base::FilePath& child) {
  base::FilePath path;
  return base::PathService::Get(key, &path) && path.IsParent(child);
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
  return LegacyAppCommandWebImpl::CreateAppCommandWeb(
      GetUpdaterScope(), base::UTF8ToWide(app_id()), command_id, command);
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
    const WCHAR* app_guid,
    const WCHAR* cmd_id,
    DWORD caller_proc_id,
    ULONG_PTR* proc_handle) {
  VLOG(2) << "LegacyProcessLauncherImpl::LaunchCmdElevated: app " << app_guid
          << ", cmd_id " << cmd_id << ", pid " << caller_proc_id;

  if (!cmd_id || !wcslen(cmd_id) || !proc_handle) {
    VLOG(1) << "Invalid arguments";
    return E_INVALIDARG;
  }

  base::win::ScopedHandle caller_proc_handle;
  HRESULT hr = OpenCallerProcessHandle(caller_proc_id, caller_proc_handle);
  if (FAILED(hr)) {
    VLOG(1) << "failed to open caller's handle " << hr;
    return hr;
  }

  std::wstring cmd = GetCommandToLaunch(app_guid, cmd_id);
  if (cmd.empty()) {
    VLOG(1) << "cmd not found";
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  VLOG(2) << "[LegacyProcessLauncherImpl::LaunchCmdElevated][cmd " << cmd
          << "]";
  return LaunchCmd(cmd, caller_proc_handle, proc_handle);
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

HRESULT LegacyAppCommandWebImpl::CreateAppCommandWeb(
    UpdaterScope scope,
    const std::wstring& app_id,
    const std::wstring& command_id,
    IDispatch** app_command_web) {
  DCHECK(app_command_web);

  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> web_impl;
  if (HRESULT hr =
          CreateLegacyAppCommandWebImpl(scope, app_id, command_id, web_impl);
      FAILED(hr)) {
    return hr;
  }

  return web_impl.CopyTo(app_command_web);
}

HRESULT LegacyAppCommandWebImpl::CreateLegacyAppCommandWebImpl(
    UpdaterScope scope,
    const std::wstring& app_id,
    const std::wstring& command_id,
    Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& web_impl) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  const std::wstring app_key_name = base::StrCat({CLIENTS_KEY, app_id});
  std::wstring command_format;

  if (const base::win::RegKey command_key(
          root,
          base::StrCat({app_key_name, L"\\", kRegKeyCommands, command_id})
              .c_str(),
          Wow6432(KEY_QUERY_VALUE));
      !command_key.Valid()) {
    const base::win::RegKey app_key(root, app_key_name.c_str(),
                                    Wow6432(KEY_QUERY_VALUE));
    if (!app_key.HasValue(command_id.c_str()))
      return HRESULT_FROM_WIN32(ERROR_BAD_COMMAND);

    // Older command layout format:
    //     Update\Clients\{`app_id`}
    //         REG_SZ `command_id` == {command format}
    if (const LONG result =
            app_key.ReadValue(command_id.c_str(), &command_format);
        result != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(result);
    }
  } else {
    // New command layout format:
    //     Update\Clients\{`app_id`}\Commands\`command_id`
    //         REG_SZ "CommandLine" == {command format}
    if (const LONG result =
            command_key.ReadValue(kRegValueCommandLine, &command_format);
        result != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(result);
    }
  }

  if (HRESULT hr =
          Microsoft::WRL::MakeAndInitialize<LegacyAppCommandWebImpl>(&web_impl);
      FAILED(hr)) {
    return hr;
  }

  return web_impl->Initialize(scope, command_format);
}

bool LegacyAppCommandWebImpl::InitializeExecutable(
    UpdaterScope scope,
    const base::FilePath& exe_path) {
  if (!exe_path.IsAbsolute() ||
      (scope == UpdaterScope::kSystem &&
       !IsParentOf(base::DIR_PROGRAM_FILESX86, exe_path) &&
       !IsParentOf(base::DIR_PROGRAM_FILES6432, exe_path))) {
    return false;
  }

  executable_ = exe_path;
  return true;
}

HRESULT LegacyAppCommandWebImpl::Initialize(UpdaterScope scope,
                                            std::wstring command_format) {
  int num_args = 0;
  ScopedLocalAlloc args(::CommandLineToArgvW(&command_format[0], &num_args));
  if (!args.is_valid() || num_args < 1)
    return E_INVALIDARG;

  const wchar_t** argv = reinterpret_cast<const wchar_t**>(args.get());
  if (!InitializeExecutable(scope, base::FilePath(argv[0])))
    return E_INVALIDARG;

  parameters_.clear();
  for (int i = 1; i < num_args; ++i)
    parameters_.push_back(argv[i]);

  return S_OK;
}

absl::optional<std::wstring> LegacyAppCommandWebImpl::FormatCommandLine(
    const std::vector<std::wstring>& parameters) const {
  std::wstring formatted_command_line;
  for (size_t i = 0; i < parameters_.size(); ++i) {
    absl::optional<std::wstring> formatted_parameter =
        FormatParameter(parameters, parameters_[i]);
    if (!formatted_parameter) {
      VLOG(1) << __func__ << " FormatParameter failed";
      return absl::nullopt;
    }

    formatted_command_line.append(
        QuoteForCommandLineToArgvW(*formatted_parameter));

    if (i + 1 < parameters_.size())
      formatted_command_line.push_back(L' ');
  }

  return formatted_command_line;
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

STDMETHODIMP LegacyAppCommandWebImpl::execute(VARIANT parameter1,
                                              VARIANT parameter2,
                                              VARIANT parameter3,
                                              VARIANT parameter4,
                                              VARIANT parameter5,
                                              VARIANT parameter6,
                                              VARIANT parameter7,
                                              VARIANT parameter8,
                                              VARIANT parameter9) {
  if (executable_.empty() || process_.IsValid()) {
    return E_UNEXPECTED;
  }

  std::vector<std::wstring> parameters;
  for (const VARIANT& parameter :
       {parameter1, parameter2, parameter3, parameter4, parameter5, parameter6,
        parameter7, parameter8, parameter9}) {
    const std::wstring parameter_string = StringFromVariant(parameter);
    if (parameter_string.empty())
      break;
    parameters.push_back(parameter_string);
  }

  absl::optional<std::wstring> command_line = FormatCommandLine(parameters);
  if (!command_line)
    return E_INVALIDARG;

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(executable_.value().c_str(), &(*command_line)[0],
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &si, &pi)) {
    return HRESULTFromLastError();
  }

  ::CloseHandle(pi.hThread);

  process_ = base::Process(pi.hProcess);
  return process_.IsValid() ? S_OK : E_UNEXPECTED;
}

STDMETHODIMP LegacyAppCommandWebImpl::GetTypeInfoCount(UINT*) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyAppCommandWebImpl::GetTypeInfo(UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyAppCommandWebImpl::GetIDsOfNames(REFIID,
                                                    LPOLESTR*,
                                                    UINT,
                                                    LCID,
                                                    DISPID*) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyAppCommandWebImpl::Invoke(DISPID,
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
