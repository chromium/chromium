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

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "chrome/updater/app/app_server_win.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/app_command_runner.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/setup/setup_util.h"
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

// Implements `IAppVersionWeb`.
class AppVersionWebImpl : public IDispatchImpl<IAppVersionWeb> {
 public:
  AppVersionWebImpl()
      : IDispatchImpl<IAppVersionWeb>(IID_MAPS_USERSYSTEM(IAppVersionWeb)) {}
  AppVersionWebImpl(const AppVersionWebImpl&) = delete;
  AppVersionWebImpl& operator=(const AppVersionWebImpl&) = delete;

  HRESULT RuntimeClassInitialize(const std::wstring& version) {
    version_ = version;

    return S_OK;
  }

  // Overrides for IAppVersionWeb.
  IFACEMETHODIMP get_version(BSTR* version) override {
    CHECK(version);

    *version = base::win::ScopedBstr(version_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_packageCount(long* count) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_packageWeb(long index, IDispatch** package) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

 private:
  ~AppVersionWebImpl() override = default;

  std::wstring version_;
};

// Implements `ICurrentState`. Initialized with a snapshot of the current state
// of the install.
class CurrentStateImpl : public IDispatchImpl<ICurrentState> {
 public:
  CurrentStateImpl()
      : IDispatchImpl<ICurrentState>(IID_MAPS_USERSYSTEM(ICurrentState)) {}
  CurrentStateImpl(const CurrentStateImpl&) = delete;
  CurrentStateImpl& operator=(const CurrentStateImpl&) = delete;

  HRESULT RuntimeClassInitialize(
      LONG state_value,
      const std::wstring& available_version,
      ULONGLONG bytes_downloaded,
      ULONGLONG total_bytes_to_download,
      LONG download_time_remaining_ms,
      ULONGLONG next_retry_time,
      LONG install_progress_percentage,
      LONG install_time_remaining_ms,
      bool is_canceled,
      LONG error_code,
      LONG extra_code1,
      const std::wstring& completion_message,
      LONG installer_result_code,
      LONG installer_result_extra_code1,
      const std::wstring& post_install_launch_command_line,
      const std::wstring& post_install_url,
      LONG post_install_action) {
    state_value_ = state_value;
    available_version_ = available_version;
    bytes_downloaded_ = bytes_downloaded;
    total_bytes_to_download_ = total_bytes_to_download;
    download_time_remaining_ms_ = download_time_remaining_ms;
    next_retry_time_ = next_retry_time;
    install_progress_percentage_ = install_progress_percentage;
    install_time_remaining_ms_ = install_time_remaining_ms;
    is_canceled_ = is_canceled ? VARIANT_TRUE : VARIANT_FALSE;
    error_code_ = error_code;
    extra_code1_ = extra_code1;
    completion_message_ = completion_message;
    installer_result_code_ = installer_result_code;
    installer_result_extra_code1_ = installer_result_extra_code1;
    post_install_launch_command_line_ = post_install_launch_command_line;
    post_install_url_ = post_install_url;
    post_install_action_ = post_install_action;

    return S_OK;
  }

  // Overrides for ICurrentState.
  IFACEMETHODIMP get_stateValue(LONG* state_value) override {
    CHECK(state_value);

    *state_value = state_value_;
    return S_OK;
  }

  IFACEMETHODIMP get_availableVersion(BSTR* available_version) override {
    CHECK(available_version);

    *available_version = base::win::ScopedBstr(available_version_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_bytesDownloaded(ULONG* bytes_downloaded) override {
    CHECK(bytes_downloaded);

    *bytes_downloaded = bytes_downloaded_;
    return S_OK;
  }

  IFACEMETHODIMP get_totalBytesToDownload(
      ULONG* total_bytes_to_download) override {
    CHECK(total_bytes_to_download);

    *total_bytes_to_download = total_bytes_to_download_;
    return S_OK;
  }

  IFACEMETHODIMP get_downloadTimeRemainingMs(
      LONG* download_time_remaining_ms) override {
    CHECK(download_time_remaining_ms);

    *download_time_remaining_ms = download_time_remaining_ms_;
    return S_OK;
  }

  IFACEMETHODIMP get_nextRetryTime(ULONGLONG* next_retry_time) override {
    CHECK(next_retry_time);

    *next_retry_time = next_retry_time_;
    return S_OK;
  }

  IFACEMETHODIMP get_installProgress(
      LONG* install_progress_percentage) override {
    CHECK(install_progress_percentage);

    *install_progress_percentage = install_progress_percentage_;
    return S_OK;
  }

  IFACEMETHODIMP get_installTimeRemainingMs(
      LONG* install_time_remaining_ms) override {
    CHECK(install_time_remaining_ms);

    *install_time_remaining_ms = install_time_remaining_ms_;
    return S_OK;
  }

  IFACEMETHODIMP get_isCanceled(VARIANT_BOOL* is_canceled) override {
    CHECK(is_canceled);

    *is_canceled = is_canceled_;
    return S_OK;
  }

  IFACEMETHODIMP get_errorCode(LONG* error_code) override {
    CHECK(error_code);

    *error_code = error_code_;
    return S_OK;
  }

  IFACEMETHODIMP get_extraCode1(LONG* extra_code1) override {
    CHECK(extra_code1);

    *extra_code1 = extra_code1_;
    return S_OK;
  }

  IFACEMETHODIMP get_completionMessage(BSTR* completion_message) override {
    CHECK(completion_message);

    *completion_message = base::win::ScopedBstr(completion_message_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_installerResultCode(LONG* installer_result_code) override {
    CHECK(installer_result_code);

    *installer_result_code = installer_result_code_;
    return S_OK;
  }

  IFACEMETHODIMP get_installerResultExtraCode1(
      LONG* installer_result_extra_code1) override {
    CHECK(installer_result_extra_code1);

    *installer_result_extra_code1 = installer_result_extra_code1_;
    return S_OK;
  }

  IFACEMETHODIMP get_postInstallLaunchCommandLine(
      BSTR* post_install_launch_command_line) override {
    CHECK(post_install_launch_command_line);

    *post_install_launch_command_line =
        base::win::ScopedBstr(post_install_launch_command_line_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_postInstallUrl(BSTR* post_install_url) override {
    CHECK(post_install_url);

    *post_install_url = base::win::ScopedBstr(post_install_url_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_postInstallAction(LONG* post_install_action) override {
    CHECK(post_install_action);

    *post_install_action = post_install_action_;
    return S_OK;
  }

 private:
  ~CurrentStateImpl() override = default;

  LONG state_value_;
  std::wstring available_version_;
  ULONGLONG bytes_downloaded_;
  ULONGLONG total_bytes_to_download_;
  LONG download_time_remaining_ms_;
  ULONGLONG next_retry_time_;
  LONG install_progress_percentage_;
  LONG install_time_remaining_ms_;
  VARIANT_BOOL is_canceled_;
  LONG error_code_;
  LONG extra_code1_;
  std::wstring completion_message_;
  LONG installer_result_code_;
  LONG installer_result_extra_code1_;
  std::wstring post_install_launch_command_line_;
  std::wstring post_install_url_;
  LONG post_install_action_;
};

// This class implements the legacy Omaha3 IAppWeb interface as expected by
// Chrome's on-demand client.
class AppWebImpl : public IDispatchImpl<IAppWeb> {
 public:
  AppWebImpl()
      : IDispatchImpl<IAppWeb>(IID_MAPS_USERSYSTEM(IAppWeb)),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}
  AppWebImpl(const AppWebImpl&) = delete;
  AppWebImpl& operator=(const AppWebImpl&) = delete;

  HRESULT RuntimeClassInitialize(
      const std::wstring& app_id,
      UpdateService::PolicySameVersionUpdate policy_same_version_update) {
    app_id_ = base::WideToASCII(app_id);
    policy_same_version_update_ = policy_same_version_update;
    return S_OK;
  }

  // For backward-compatibility purposes, the `CheckForUpdate` call assumes
  // foreground priority and disallows same version updates.
  HRESULT CheckForUpdate() {
    using AppWebImplPtr = Microsoft::WRL::ComPtr<AppWebImpl>;
    scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
    com_server->main_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<UpdateService> update_service, AppWebImplPtr obj) {
              update_service->CheckForUpdate(
                  obj->app_id_, UpdateService::Priority::kForeground,
                  obj->policy_same_version_update_,
                  base::BindRepeating(
                      [](AppWebImplPtr obj,
                         const UpdateService::UpdateState& state_update) {
                        obj->task_runner_->PostTask(
                            FROM_HERE,
                            base::BindOnce(&AppWebImpl::UpdateStateCallback,
                                           obj, state_update));
                      },
                      obj),
                  base::BindOnce(
                      [](AppWebImplPtr obj, UpdateService::Result result) {
                        obj->task_runner_->PostTask(
                            FROM_HERE,
                            base::BindOnce(&AppWebImpl::UpdateResultCallback,
                                           obj, result));
                      },
                      obj));
            },
            com_server->update_service(), AppWebImplPtr(this)));
    return S_OK;
  }

  HRESULT Update() {
    using AppWebImplPtr = Microsoft::WRL::ComPtr<AppWebImpl>;
    scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
    com_server->main_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<UpdateService> update_service, AppWebImplPtr obj) {
              update_service->Update(
                  obj->app_id_, "", UpdateService::Priority::kForeground,
                  obj->policy_same_version_update_,
                  base::BindRepeating(
                      [](AppWebImplPtr obj,
                         const UpdateService::UpdateState& state_update) {
                        obj->task_runner_->PostTask(
                            FROM_HERE,
                            base::BindOnce(&AppWebImpl::UpdateStateCallback,
                                           obj, state_update));
                      },
                      obj),
                  base::BindOnce(
                      [](AppWebImplPtr obj, UpdateService::Result result) {
                        obj->task_runner_->PostTask(
                            FROM_HERE,
                            base::BindOnce(&AppWebImpl::UpdateResultCallback,
                                           obj, result));
                      },
                      obj));
            },
            com_server->update_service(), AppWebImplPtr(this)));
    return S_OK;
  }

  // Overrides for IAppWeb.
  IFACEMETHODIMP get_appId(BSTR* app_id) override {
    CHECK(app_id);

    *app_id = base::win::ScopedBstr(base::ASCIIToWide(app_id_)).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_currentVersionWeb(IDispatch** current) override {
    // Holds the result of the IPC to retrieve the current version.
    struct CurrentVersionResult
        : public base::RefCountedThreadSafe<CurrentVersionResult> {
      absl::optional<base::Version> current_version;
      base::WaitableEvent completion_event;

     private:
      friend class base::RefCountedThreadSafe<CurrentVersionResult>;
      virtual ~CurrentVersionResult() = default;
    };

    auto result = base::MakeRefCounted<CurrentVersionResult>();
    GetAppServerWinInstance()->main_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const std::string app_id,
               scoped_refptr<CurrentVersionResult> result) {
              const base::ScopedClosureRunner signal_event(base::BindOnce(
                  [](scoped_refptr<CurrentVersionResult> result) {
                    result->completion_event.Signal();
                  },
                  result));

              const base::Version current_version =
                  base::MakeRefCounted<const PersistedData>(
                      GetUpdaterScope(),
                      GetAppServerWinInstance()->prefs()->GetPrefService())
                      ->GetProductVersion(app_id);
              if (!current_version.IsValid()) {
                return;
              }

              result->current_version = current_version;
            },
            app_id_, result));

    if (!result->completion_event.TimedWait(base::Seconds(60)) ||
        !result->current_version.has_value()) {
      return E_FAIL;
    }

    return MakeAndInitializeComObject<AppVersionWebImpl>(
        current, base::ASCIIToWide(result->current_version->GetString()));
  }

  IFACEMETHODIMP get_nextVersionWeb(IDispatch** next) override {
    base::AutoLock lock{lock_};

    if (!state_update_ || !state_update_->next_version.IsValid()) {
      return E_FAIL;
    }

    return MakeAndInitializeComObject<AppVersionWebImpl>(
        next, base::ASCIIToWide(state_update_->next_version.GetString()));
  }

  IFACEMETHODIMP get_command(BSTR command_id, IDispatch** command) override {
    return MakeAndInitializeComObject<LegacyAppCommandWebImpl>(
        command, GetUpdaterScope(), base::UTF8ToWide(app_id_), command_id);
  }

  IFACEMETHODIMP cancel() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_currentState(IDispatch** current_state) override {
    CHECK(current_state);

    base::AutoLock lock{lock_};

    LONG state_value = STATE_INIT;
    std::wstring available_version;
    ULONG bytes_downloaded = -1;
    ULONG total_bytes_to_download = -1;
    LONG install_progress_percentage = -1;
    LONG error_code = 0;
    LONG extra_code1 = 0;
    std::wstring completion_message;
    LONG installer_result_code = 0;

    if (state_update_) {
      // `state_value` is set to the state of update as seen by the on-demand
      // client:
      // - if the repeating callback has been received: set to the specific
      // state.
      // - if the completion callback has been received, but no repeating
      // callback, then it is set to STATE_ERROR. This is an error state and it
      // indicates that update is not going to be further handled and repeating
      // callbacks posted.
      // - if no callback has been received at all: set to STATE_INIT.
      switch (state_update_.value().state) {
        case UpdateService::UpdateState::State::kUnknown:  // Fall
                                                           // through.
        case UpdateService::UpdateState::State::kNotStarted:
          state_value = STATE_INIT;
          break;
        case UpdateService::UpdateState::State::kCheckingForUpdates:
          state_value = STATE_CHECKING_FOR_UPDATE;
          break;
        case UpdateService::UpdateState::State::kUpdateAvailable:
          state_value = STATE_UPDATE_AVAILABLE;
          break;
        case UpdateService::UpdateState::State::kDownloading:
          state_value = STATE_DOWNLOADING;
          break;
        case UpdateService::UpdateState::State::kInstalling:
          state_value = STATE_INSTALLING;
          break;
        case UpdateService::UpdateState::State::kUpdated:
          state_value = STATE_INSTALL_COMPLETE;
          break;
        case UpdateService::UpdateState::State::kNoUpdate:
          state_value = STATE_NO_UPDATE;
          break;
        case UpdateService::UpdateState::State::kUpdateError:
          state_value = STATE_ERROR;
          break;
      }

      available_version =
          base::UTF8ToWide(state_update_->next_version.GetString());
      bytes_downloaded = state_update_->downloaded_bytes;
      total_bytes_to_download = state_update_->total_bytes;
      install_progress_percentage = state_update_->install_progress;

      if (state_update_->state ==
          UpdateService::UpdateState::State::kUpdateError) {
        error_code = state_update_->error_code;
        extra_code1 = state_update_->extra_code1;

        if (state_update_->error_code == kErrorApplicationInstallerFailed) {
          // In the error case, if an installer error occurred, it remaps the
          // installer error to the legacy installer error value, for backward
          // compatibility.
          error_code = GOOPDATEINSTALL_E_INSTALLER_FAILED;

          // TODO(crbug.com/1447293): this string needs localization.
          completion_message = L"Installer failed.";
          installer_result_code = state_update_->extra_code1;
        }
      }

    } else if (result_) {
      CHECK_NE(result_.value(), UpdateService::Result::kSuccess);
      state_value = STATE_ERROR;
      error_code =
          (result_.value() == UpdateService::Result::kSuccess) ? 0 : -1;
    }

    return MakeAndInitializeComObject<CurrentStateImpl>(
        current_state, state_value, available_version, bytes_downloaded,
        total_bytes_to_download,
        /*download_time_remaining_ms=*/-1,
        /*next_retry_time=*/-1, install_progress_percentage,
        /*install_time_remaining_ms=*/-1,
        /*is_canceled=*/VARIANT_FALSE, error_code, extra_code1,
        completion_message, installer_result_code,
        /*installer_result_extra_code1=*/-1,
        /*post_install_launch_command_line=*/L"",
        /*post_install_url=*/L"",
        /*post_install_action=*/0);
  }

  IFACEMETHODIMP launch() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP uninstall() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_serverInstallDataIndex(BSTR* language) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP put_serverInstallDataIndex(BSTR language) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

 private:
  ~AppWebImpl() override = default;

  void UpdateStateCallback(UpdateService::UpdateState state_update) {
    base::AutoLock lock{lock_};
    state_update_ = state_update;
  }

  void UpdateResultCallback(UpdateService::Result result) {
    base::AutoLock lock{lock_};
    result_ = result;
  }

  // Handles the update service callbacks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::string app_id_;
  UpdateService::PolicySameVersionUpdate policy_same_version_update_ =
      UpdateService::PolicySameVersionUpdate::kNotAllowed;

  // Access to `state_update_` and `result_` must be serialized by using the
  // lock.
  mutable base::Lock lock_;
  absl::optional<UpdateService::UpdateState> state_update_;
  absl::optional<UpdateService::Result> result_;
};

// This class implements the legacy Omaha3 IAppBundleWeb interface as expected
// by Chrome's on-demand client.
class AppBundleWebImpl : public IDispatchImpl<IAppBundleWeb> {
 public:
  AppBundleWebImpl()
      : IDispatchImpl<IAppBundleWeb>(IID_MAPS_USERSYSTEM(IAppBundleWeb)) {}
  AppBundleWebImpl(const AppBundleWebImpl&) = delete;
  AppBundleWebImpl& operator=(const AppBundleWebImpl&) = delete;

  HRESULT RuntimeClassInitialize() { return S_OK; }

  // Overrides for IAppBundleWeb.
  IFACEMETHODIMP createApp(BSTR app_id,
                           BSTR brand_code,
                           BSTR language,
                           BSTR ap) override {
    base::AutoLock lock{lock_};

    if (app_web_) {
      return E_UNEXPECTED;
    }

    is_install_ = true;
    return MakeAndInitializeComObject<AppWebImpl>(
        app_web_, app_id, UpdateService::PolicySameVersionUpdate::kAllowed);
  }

  IFACEMETHODIMP createInstalledApp(BSTR app_id) override {
    base::AutoLock lock{lock_};

    if (app_web_) {
      return E_UNEXPECTED;
    }

    is_install_ = false;
    return MakeAndInitializeComObject<AppWebImpl>(
        app_web_, app_id, UpdateService::PolicySameVersionUpdate::kNotAllowed);
  }

  IFACEMETHODIMP createAllInstalledApps() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_displayLanguage(BSTR* language) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP put_displayLanguage(BSTR language) override { return S_OK; }

  IFACEMETHODIMP put_parentHWND(ULONG_PTR hwnd) override { return S_OK; }

  IFACEMETHODIMP get_length(int* number) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_appWeb(int index, IDispatch** app_web) override {
    base::AutoLock lock{lock_};

    if (index != 0 || !app_web_) {
      return E_UNEXPECTED;
    }

    return app_web_.CopyTo(app_web);
  }

  IFACEMETHODIMP initialize() override { return S_OK; }

  IFACEMETHODIMP checkForUpdate() override {
    base::AutoLock lock{lock_};
    if (!app_web_) {
      return E_UNEXPECTED;
    }
    return app_web_->CheckForUpdate();
  }

  IFACEMETHODIMP download() override {
    VLOG(1) << "`install()` implements the download: " << __func__;
    return S_OK;
  }

  IFACEMETHODIMP install() override {
    base::AutoLock lock{lock_};

    if (!app_web_) {
      return E_UNEXPECTED;
    }

    if (is_install_ && FAILED(IsCOMCallerAllowed())) {
      VLOG(1) << __func__ << ": admin rights required for new system installs";
      return E_ACCESSDENIED;
    }

    return app_web_->Update();
  }

  IFACEMETHODIMP pause() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP resume() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP cancel() override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP downloadPackage(BSTR app_id, BSTR package_name) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_currentState(VARIANT* current_state) override {
    LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
    return E_NOTIMPL;
  }

 private:
  ~AppBundleWebImpl() override = default;

  // Access to the object members must be serialized by using the lock.
  mutable base::Lock lock_;

  // Only a single app at a time is supported.
  Microsoft::WRL::ComPtr<AppWebImpl> app_web_;

  // `false` for updates. `true` for fresh installs or reinstalls.
  bool is_install_ = false;
};

LegacyOnDemandImpl::LegacyOnDemandImpl()
    : IDispatchImpl<IGoogleUpdate3Web>(IID_MAPS_USERSYSTEM(IGoogleUpdate3Web)) {
}

LegacyOnDemandImpl::~LegacyOnDemandImpl() = default;

STDMETHODIMP LegacyOnDemandImpl::createAppBundleWeb(
    IDispatch** app_bundle_web) {
  CHECK(app_bundle_web);
  return MakeAndInitializeComObject<AppBundleWebImpl>(app_bundle_web);
}

LegacyProcessLauncherImpl::LegacyProcessLauncherImpl() = default;
LegacyProcessLauncherImpl::~LegacyProcessLauncherImpl() = default;

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdLine(const WCHAR* cmd_line) {
  return LaunchCmdLineEx(cmd_line, nullptr, nullptr, nullptr);
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchBrowser(DWORD browser_type,
                                                      const WCHAR* url) {
  LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
  return E_NOTIMPL;
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdElevated(
    const WCHAR* app_id,
    const WCHAR* command_id,
    DWORD caller_proc_id,
    ULONG_PTR* proc_handle) {
  ASSIGN_OR_RETURN(auto app_command_runner,
                   AppCommandRunner::LoadAppCommand(UpdaterScope::kSystem,
                                                    app_id, command_id));

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
          PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, 0)) {
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
  LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
  return E_NOTIMPL;
}

LegacyAppCommandWebImpl::LegacyAppCommandWebImpl()
    : IDispatchImpl<IAppCommandWeb>(IID_MAPS_USERSYSTEM(IAppCommandWeb)) {}
LegacyAppCommandWebImpl::~LegacyAppCommandWebImpl() = default;

HRESULT LegacyAppCommandWebImpl::RuntimeClassInitialize(
    UpdaterScope scope,
    const std::wstring& app_id,
    const std::wstring& command_id) {
  app_command_runner_ =
      AppCommandRunner::LoadAppCommand(scope, app_id, command_id);
  return app_command_runner_.error_or(S_OK);
}

STDMETHODIMP LegacyAppCommandWebImpl::get_status(UINT* status) {
  CHECK(status);

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
  CHECK(exit_code);

  int code = -1;
  if (!process_.IsValid() ||
      !process_.WaitForExitWithTimeout(base::TimeDelta(), &code)) {
    return E_FAIL;
  }

  *exit_code = code;
  return S_OK;
}

STDMETHODIMP LegacyAppCommandWebImpl::get_output(BSTR* output) {
  LOG(ERROR) << "Reached unimplemented COM method: " << __func__;
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
  CHECK(app_command_runner_.has_value());

  std::vector<std::wstring> substitutions;
  for (const VARIANT& substitution :
       {substitution1, substitution2, substitution3, substitution4,
        substitution5, substitution6, substitution7, substitution8,
        substitution9}) {
    const absl::optional<std::wstring> substitution_string =
        StringFromVariant(substitution);
    if (!substitution_string) {
      break;
    }

    VLOG(2) << __func__
            << " substitution_string: " << substitution_string.value();
    substitutions.push_back(substitution_string.value());
  }

  return app_command_runner_->Run(substitutions, process_);
}

PolicyStatusImpl::PolicyStatusImpl()
    : IDispatchImpl<IPolicyStatus3, IPolicyStatus2, IPolicyStatus>(
          {IID_MAP_ENTRY_USER(IPolicyStatus3),
           IID_MAP_ENTRY_USER(IPolicyStatus2),
           IID_MAP_ENTRY_USER(IPolicyStatus)},
          {IID_MAP_ENTRY_SYSTEM(IPolicyStatus3),
           IID_MAP_ENTRY_SYSTEM(IPolicyStatus2),
           IID_MAP_ENTRY_SYSTEM(IPolicyStatus)}),
      policy_service_(GetAppServerWinInstance()->config()->GetPolicyService()) {
}
PolicyStatusImpl::~PolicyStatusImpl() = default;

HRESULT PolicyStatusImpl::RuntimeClassInitialize() {
  return S_OK;
}

// IPolicyStatus.
STDMETHODIMP PolicyStatusImpl::get_lastCheckPeriodMinutes(DWORD* minutes) {
  CHECK(minutes);

  PolicyStatus<base::TimeDelta> period = policy_service_->GetLastCheckPeriod();
  if (!period) {
    return E_FAIL;
  }

  *minutes = period.policy().InMinutes();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_updatesSuppressedTimes(
    DWORD* start_hour,
    DWORD* start_min,
    DWORD* duration_min,
    VARIANT_BOOL* are_updates_suppressed) {
  CHECK(start_hour);
  CHECK(start_min);
  CHECK(duration_min);
  CHECK(are_updates_suppressed);

  PolicyStatus<UpdatesSuppressedTimes> updates_suppressed_times =
      policy_service_->GetUpdatesSuppressedTimes();
  if (!updates_suppressed_times || !updates_suppressed_times.policy().valid()) {
    return E_FAIL;
  }

  *start_hour = updates_suppressed_times.policy().start_hour_;
  *start_min = updates_suppressed_times.policy().start_minute_;
  *duration_min = updates_suppressed_times.policy().duration_minute_;
  *are_updates_suppressed =
      policy_service_->AreUpdatesSuppressedNow() ? VARIANT_TRUE : VARIANT_FALSE;

  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_downloadPreferenceGroupPolicy(BSTR* pref) {
  CHECK(pref);

  PolicyStatus<std::string> download_preference =
      policy_service_->GetDownloadPreferenceGroupPolicy();
  if (!download_preference) {
    return E_FAIL;
  }

  *pref = base::win::ScopedBstr(base::ASCIIToWide(download_preference.policy()))
              .Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheSizeLimitMBytes(DWORD* limit) {
  CHECK(limit);

  PolicyStatus<int> cache_size_limit =
      policy_service_->GetPackageCacheSizeLimitMBytes();
  if (!cache_size_limit) {
    return E_FAIL;
  }

  *limit = cache_size_limit.policy();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheExpirationTimeDays(DWORD* days) {
  CHECK(days);

  PolicyStatus<int> cache_life_limit =
      policy_service_->GetPackageCacheExpirationTimeDays();
  if (!cache_life_limit) {
    return E_FAIL;
  }

  *days = cache_life_limit.policy();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppInstalls(
    BSTR app_id,
    DWORD* policy) {
  CHECK(policy);

  PolicyStatus<int> install_policy =
      policy_service_->GetPolicyForAppInstalls(base::WideToASCII(app_id));
  if (!install_policy) {
    return E_FAIL;
  }

  *policy = install_policy.policy();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppUpdates(BSTR app_id,
                                                                DWORD* policy) {
  CHECK(policy);

  PolicyStatus<int> update_policy =
      policy_service_->GetPolicyForAppUpdates(base::WideToASCII(app_id));
  if (!update_policy) {
    return E_FAIL;
  }

  *policy = update_policy.policy();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_targetVersionPrefix(BSTR app_id,
                                                       BSTR* prefix) {
  CHECK(prefix);

  PolicyStatus<std::string> target_version_prefix =
      policy_service_->GetTargetVersionPrefix(base::WideToASCII(app_id));
  if (!target_version_prefix) {
    return E_FAIL;
  }

  *prefix =
      base::win::ScopedBstr(base::ASCIIToWide(target_version_prefix.policy()))
          .Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_isRollbackToTargetVersionAllowed(
    BSTR app_id,
    VARIANT_BOOL* rollback_allowed) {
  CHECK(rollback_allowed);

  PolicyStatus<bool> is_rollback_allowed =
      policy_service_->IsRollbackToTargetVersionAllowed(
          base::WideToASCII(app_id));
  if (!is_rollback_allowed) {
    return E_FAIL;
  }

  *rollback_allowed =
      is_rollback_allowed.policy() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_updaterVersion(BSTR* version) {
  CHECK(version);

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
  using ValueGetter = base::RepeatingCallback<PolicyStatus<T>()>;

  static auto Get(ValueGetter value_getter) {
    auto result = base::WrapRefCounted(new PolicyStatusResult<T>(value_getter));
    GetAppServerWinInstance()->main_task_runner()->PostTask(
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
    PolicyStatus<T> policy_status = value_getter.Run();
    if (policy_status) {
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
  CHECK(last_checked);

  using PolicyStatusImplPtr = Microsoft::WRL::ComPtr<PolicyStatusImpl>;
  auto result = base::MakeRefCounted<LastCheckedTimeResult>();
  GetAppServerWinInstance()->main_task_runner()->PostTask(
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
                    GetUpdaterScope(),
                    GetAppServerWinInstance()->prefs()->GetPrefService())
                    ->GetLastChecked();
            if (last_checked_time.is_null()) {
              return;
            }

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
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateService::FetchPolicies,
                     com_server->update_service(),
                     base::DoNothingWithBoundArgs(PolicyStatusImplPtr(this))));
  return S_OK;
}

STDMETHODIMP PolicyStatusImpl::get_lastCheckPeriodMinutes(
    IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(base::BindRepeating(
      &PolicyService::DeprecatedGetLastCheckPeriodMinutes, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_updatesSuppressedTimes(
    IPolicyStatusValue** value,
    VARIANT_BOOL* are_updates_suppressed) {
  CHECK(value);
  CHECK(are_updates_suppressed);

  auto policy_status =
      PolicyStatusResult<UpdatesSuppressedTimes>::Get(base::BindRepeating(
          &PolicyService::GetUpdatesSuppressedTimes, policy_service_));
  if (!policy_status.has_value()) {
    return E_FAIL;
  }
  const UpdatesSuppressedTimes updates_suppressed_times =
      policy_status->effective_policy()->policy;
  if (!updates_suppressed_times.valid()) {
    return E_FAIL;
  }
  *are_updates_suppressed =
      policy_service_->AreUpdatesSuppressedNow() ? VARIANT_TRUE : VARIANT_FALSE;
  return PolicyStatusValueImpl::Create(*policy_status, value);
}

STDMETHODIMP PolicyStatusImpl::get_downloadPreferenceGroupPolicy(
    IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(base::BindRepeating(
      &PolicyService::GetDownloadPreferenceGroupPolicy, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheSizeLimitMBytes(
    IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(base::BindRepeating(
      &PolicyService::GetPackageCacheSizeLimitMBytes, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_packageCacheExpirationTimeDays(
    IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(base::BindRepeating(
      &PolicyService::GetPackageCacheExpirationTimeDays, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_proxyMode(IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetProxyMode, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_proxyPacUrl(IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetProxyPacUrl, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_proxyServer(IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<std::string>::Get(
      base::BindRepeating(&PolicyService::GetProxyServer, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppInstalls(
    BSTR app_id,
    IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(
      base::BindRepeating(&PolicyService::GetPolicyForAppInstalls,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_effectivePolicyForAppUpdates(
    BSTR app_id,
    IPolicyStatusValue** value) {
  CHECK(value);
  auto policy_status = PolicyStatusResult<int>::Get(
      base::BindRepeating(&PolicyService::GetPolicyForAppUpdates,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_targetVersionPrefix(
    BSTR app_id,
    IPolicyStatusValue** value) {
  CHECK(value);
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
  CHECK(value);
  auto policy_status = PolicyStatusResult<bool>::Get(
      base::BindRepeating(&PolicyService::IsRollbackToTargetVersionAllowed,
                          policy_service_, base::WideToASCII(app_id)));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

STDMETHODIMP PolicyStatusImpl::get_targetChannel(BSTR app_id,
                                                 IPolicyStatusValue** value) {
  CHECK(value);
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
  CHECK(value);
  auto policy_status =
      PolicyStatusResult<std::vector<std::string>>::Get(base::BindRepeating(
          &PolicyService::GetForceInstallApps, policy_service_));
  return policy_status.has_value()
             ? PolicyStatusValueImpl::Create(*policy_status, value)
             : E_FAIL;
}

PolicyStatusValueImpl::PolicyStatusValueImpl()
    : IDispatchImpl<IPolicyStatusValue>(
          {{__uuidof(IPolicyStatusValueUser), __uuidof(IPolicyStatusValue)}},
          {{__uuidof(IPolicyStatusValueSystem),
            __uuidof(IPolicyStatusValue)}}) {}
PolicyStatusValueImpl::~PolicyStatusValueImpl() = default;

template <typename T>
[[nodiscard]] HRESULT PolicyStatusValueImpl::Create(
    const T& value,
    IPolicyStatusValue** policy_status_value) {
  return MakeAndInitializeComObject<PolicyStatusValueImpl>(
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
  CHECK(source);

  *source = base::win::ScopedBstr(source_).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_value(BSTR* value) {
  CHECK(value);

  *value = base::win::ScopedBstr(value_).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_hasConflict(
    VARIANT_BOOL* has_conflict) {
  CHECK(has_conflict);

  *has_conflict = has_conflict_;
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_conflictSource(BSTR* conflict_source) {
  CHECK(conflict_source);

  *conflict_source = base::win::ScopedBstr(conflict_source_).Release();
  return S_OK;
}

STDMETHODIMP PolicyStatusValueImpl::get_conflictValue(BSTR* conflict_value) {
  CHECK(conflict_value);

  *conflict_value = base::win::ScopedBstr(conflict_value_).Release();
  return S_OK;
}

}  // namespace updater
