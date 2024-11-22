// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include <ocidl.h>
#include <windows.h>

#include <olectl.h>
#include <shldisp.h>
#include <shlobj.h>
#include <winhttp.h>
#include <wrl/client.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/elevation_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/app/app_install_util_win.h"
#include "chrome/updater/app/app_install_win_internal.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/progress_sampler.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "chrome/updater/win/manifest_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/win_constants.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_client_errors.h"
#include "url/gurl.h"

namespace updater {
namespace {

std::wstring GetTextForUpdateClientInstallError(int error_code) {
#define INSTALL_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):     \
    return GetLocalizedStringF(IDS_GENERIC_INSTALL_ERROR_BASE, L#error_code)

  switch (error_code) {
    INSTALL_SWITCH_ENTRY(update_client::InstallError::NONE);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::FINGERPRINT_WRITE_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::BAD_MANIFEST);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::GENERIC_ERROR);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::MOVE_FILES_ERROR);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::SET_PERMISSIONS_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::INVALID_VERSION);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::VERSION_NOT_UPGRADED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::NO_DIR_COMPONENT_USER);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::CLEAN_INSTALL_DIR_FAILED);
    INSTALL_SWITCH_ENTRY(
        update_client::InstallError::INSTALL_VERIFICATION_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::MISSING_INSTALL_PARAMS);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::LAUNCH_PROCESS_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::CUSTOM_ERROR_BASE);
    default:
      return GetLocalizedStringF(IDS_GENERIC_INSTALL_ERROR_BASE,
                                 GetTextForSystemError(error_code));
  }
#undef INSTALL_SWITCH_ENTRY
}

std::wstring GetTextForDownloadError(int error) {
#define DOWNLOAD_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):      \
    return GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE, L#error_code)

  switch (error) {
    DOWNLOAD_SWITCH_ENTRY(update_client::CrxDownloaderError::NO_URL);
    DOWNLOAD_SWITCH_ENTRY(update_client::CrxDownloaderError::NO_HASH);
    DOWNLOAD_SWITCH_ENTRY(
        update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS);
    DOWNLOAD_SWITCH_ENTRY(update_client::CrxDownloaderError::GENERIC_ERROR);

    case static_cast<int>(update_client::CrxDownloaderError::BAD_HASH):
      return GetLocalizedString(IDS_DOWNLOAD_HASH_MISMATCH_BASE);

    default:
      return GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE,
                                 GetTextForSystemError(error));
  }
#undef DOWNLOAD_SWITCH_ENTRY
}

std::wstring GetTextForUnpackError(int error) {
#define UNPACK_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):    \
    return GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE, L#error_code)
#define UNPACK_CACHING_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):            \
    return GetLocalizedStringF(IDS_UNPACK_CACHING_ERROR_BASE, L#error_code)

  switch (error) {
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kInvalidParams);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kInvalidFile);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kUnzipPathError);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kUnzipFailed);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kBadManifest);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kBadExtension);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kIoError);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kDeltaVerificationFailure);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kDeltaBadCommands);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kDeltaUnsupportedCommand);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kDeltaOperationFailure);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kDeltaPatchProcessFailure);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kDeltaMissingExistingFile);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kPuffinMissingPreviousCrx);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kCrxCacheNotProvided);

    UNPACK_CACHING_SWITCH_ENTRY(
        update_client::UnpackerError::kFailedToAddToCache);
    UNPACK_CACHING_SWITCH_ENTRY(
        update_client::UnpackerError::kFailedToCreateCacheDir);

    default:
      return GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                 GetTextForSystemError(error));
  }
#undef UNPACK_SWITCH_ENTRY
#undef UNPACK_CACHING_SWITCH_ENTRY
}

std::wstring GetTextForServiceError(int error) {
#define SERVICE_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):     \
    return GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE, L#error_code)

  switch (error) {
    SERVICE_SWITCH_ENTRY(update_client::ServiceError::SERVICE_WAIT_FAILED);
    SERVICE_SWITCH_ENTRY(update_client::ServiceError::UPDATE_DISABLED);
    SERVICE_SWITCH_ENTRY(update_client::ServiceError::CHECK_FOR_UPDATE_ONLY);

    case static_cast<int>(update_client::ServiceError::CANCELLED):
      return GetLocalizedString(IDS_SERVICE_ERROR_CANCELLED_BASE);

    default:
      return GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE,
                                 GetTextForSystemError(error));
  }
#undef SERVICE_SWITCH_ENTRY
}

std::wstring GetTextForUpdateCheckError(int error) {
#define UPDATE_CHECK_SWITCH_ENTRY(error_code)                       \
  case static_cast<int>(error_code):                                \
    return GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE, \
                               L#error_code)

  switch (error) {
    UPDATE_CHECK_SWITCH_ENTRY(
        update_client::ProtocolError::RESPONSE_NOT_TRUSTED);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::MISSING_PUBLIC_KEY);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::MISSING_URLS);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::PARSE_FAILED);
    UPDATE_CHECK_SWITCH_ENTRY(
        update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::URL_FETCHER_FAILED);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::INVALID_APPID);

    case static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION):
      return GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE);

    case static_cast<int>(update_client::ProtocolError::RESTRICTED_APPLICATION):
      return GetLocalizedString(IDS_RESTRICTED_RESPONSE_FROM_SERVER_BASE);

    case static_cast<int>(update_client::ProtocolError::OS_NOT_SUPPORTED):
      return GetLocalizedString(IDS_OS_NOT_SUPPORTED_BASE);

    case static_cast<int>(update_client::ProtocolError::HW_NOT_SUPPORTED):
      return GetLocalizedString(IDS_HW_NOT_SUPPORTED_BASE);

    case static_cast<int>(update_client::ProtocolError::NO_HASH):
      return GetLocalizedString(IDS_NO_HASH_BASE);

    case static_cast<int>(update_client::ProtocolError::UNSUPPORTED_PROTOCOL):
      return GetLocalizedString(IDS_UNSUPPORTED_PROTOCOL_BASE);

    case static_cast<int>(update_client::ProtocolError::INTERNAL):
      return GetLocalizedString(IDS_INTERNAL_BASE);

    // Http Status Code `401` Unauthorized.
    case 401:
      return GetLocalizedString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED_BASE);

    // Http Status Code `403` Forbidden.
    case 403:
      return GetLocalizedString(IDS_ERROR_HTTPSTATUS_FORBIDDEN_BASE);

    // Http Status Code `407` Proxy Authentication Required.
    case 407:
      return GetLocalizedString(IDS_ERROR_HTTPSTATUS_PROXY_AUTH_REQUIRED_BASE);

    case HRESULT_FROM_WIN32(ERROR_WINHTTP_NAME_NOT_RESOLVED):
      return GetLocalizedStringF(IDS_NO_NETWORK_PRESENT_ERROR_BASE,
                                 GetExecutableRelativePath().value());
    default:
      return GetLocalizedStringF(
          IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
          error >= 400 && error < 600
              ? base::UTF8ToWide(base::StringPrintf("HTTP %d", error))
              : GetTextForSystemError(error));
  }
#undef UPDATE_CHECK_SWITCH_ENTRY
}

std::wstring GetTextForInstallerError(int error_code) {
#define POLICY_ERROR_SWITCH_ENTRY(error_code)                                 \
  case error_code:                                                            \
    return GetLocalizedStringF(IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE, \
                               L#error_code)

  switch (error_code) {
    POLICY_ERROR_SWITCH_ENTRY(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY);
    POLICY_ERROR_SWITCH_ENTRY(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY);
    POLICY_ERROR_SWITCH_ENTRY(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL);

    case GOOPDATEINSTALL_E_FILENAME_INVALID:
      return GetLocalizedString(IDS_INVALID_INSTALLER_FILENAME_BASE);

    case GOOPDATEINSTALL_E_INSTALLER_FAILED_START:
      return GetLocalizedString(IDS_INSTALLER_FAILED_TO_START_BASE);

    case GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT:
      return GetLocalizedString(IDS_INSTALLER_TIMED_OUT_BASE);

    case GOOPDATEINSTALL_E_INSTALL_ALREADY_RUNNING:
      return GetLocalizedStringF(
          IDS_GENERIC_INSTALLER_ERROR_BASE,
          GetTextForSystemError(ERROR_INSTALL_ALREADY_RUNNING));

    case ERROR_SUCCESS_REBOOT_INITIATED:
    case ERROR_SUCCESS_REBOOT_REQUIRED:
    case ERROR_SUCCESS_RESTART_REQUIRED:
      return GetLocalizedStringF(IDS_INSTALL_REBOOT_BASE,
                                 GetTextForSystemError(error_code));

    default:
      return GetLocalizedStringF(IDS_GENERIC_INSTALLER_ERROR_BASE,
                                 GetTextForSystemError(error_code));
  }
#undef POLICY_ERROR_SWITCH_ENTRY
}

class InstallProgressSilentObserver : public AppInstallProgress {
 public:
  explicit InstallProgressSilentObserver(ui::OmahaWndEvents* events_sink);
  ~InstallProgressSilentObserver() override = default;

  // Overrides for AppInstallProgress.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const std::string& app_id,
                         const std::u16string& app_name,
                         const base::Version& version) override;
  void OnWaitingToDownload(const std::string& app_id,
                           const std::u16string& app_name) override;
  void OnDownloading(const std::string& app_id,
                     const std::u16string& app_name,
                     std::optional<base::TimeDelta> time_remaining,
                     int pos) override;
  void OnWaitingRetryDownload(const std::string& app_id,
                              const std::u16string& app_name,
                              base::Time next_retry_time) override;
  void OnWaitingToInstall(const std::string& app_id,
                          const std::u16string& app_name) override;
  void OnInstalling(const std::string& app_id,
                    const std::u16string& app_name,
                    std::optional<base::TimeDelta> time_remaining,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Event sink must out-live this observer.
  raw_ptr<ui::OmahaWndEvents> events_sink_ = nullptr;
};

InstallProgressSilentObserver::InstallProgressSilentObserver(
    ui::OmahaWndEvents* events_sink)
    : events_sink_(events_sink) {
  CHECK(events_sink_);
}

void InstallProgressSilentObserver::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnUpdateAvailable(
    const std::string& app_id,
    const std::u16string& app_name,
    const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnWaitingToDownload(
    const std::string& app_id,
    const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnDownloading(
    const std::string& app_id,
    const std::u16string& app_name,
    std::optional<base::TimeDelta> time_remaining,
    int pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnWaitingRetryDownload(
    const std::string& app_id,
    const std::u16string& app_name,
    base::Time next_retry_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnWaitingToInstall(
    const std::string& app_id,
    const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnInstalling(
    const std::string& app_id,
    const std::u16string& app_name,
    std::optional<base::TimeDelta> time_remaining,
    int pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InstallProgressSilentObserver::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(events_sink_);
  VLOG(1) << __func__;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAlwaysLaunchCmdSwitch)) {
    auto scoped_com_initializer =
        std::make_unique<base::win::ScopedCOMInitializer>(
            base::win::ScopedCOMInitializer::kMTA);
    LaunchCmdLines(observer_info);
  }

  events_sink_->DoExit();
}

// Implements a simple inter-thread communication protocol based on Windows
// messages exchanged between the application installer and its UI.
//
// Since the installer code and the UI code execute on different sequences, the
// installer can't invoke directly functions exposed by the UI.
class AppInstallProgressIPC : public AppInstallProgress {
 public:
  // Used as an inter-thread communication mechanism between the installer and
  // UI threads.
  static constexpr unsigned int WM_PROGRESS_WINDOW_IPC = WM_APP + 1;

  AppInstallProgressIPC(AppInstallProgress* observer, DWORD observer_thread_id)
      : observer_(observer), observer_thread_id_(observer_thread_id) {
    CHECK(observer);
  }

  AppInstallProgressIPC(const AppInstallProgressIPC&) = delete;
  AppInstallProgressIPC& operator=(const AppInstallProgressIPC&) = delete;
  ~AppInstallProgressIPC() override = default;

  // Called by the window proc when a specific application message is processed
  // by the progress window. This call always occurs in the context of the
  // thread which owns the window.
  void Invoke(WPARAM wparam, LPARAM lparam) {
    CHECK_EQ(observer_thread_id_, ::GetCurrentThreadId());
    CHECK_NE(lparam, 0);
    CHECK_EQ(wparam, WPARAM{0});
    std::unique_ptr<base::OnceClosure> callback_wrapper(
        reinterpret_cast<base::OnceClosure*>(lparam));
    std::move(*callback_wrapper).Run();
  }

  // Overrides for AppInstallProgress.
  void OnCheckingForUpdate() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(observer_);
    PostClosure(base::BindOnce(&AppInstallProgress::OnCheckingForUpdate,
                               base::Unretained(observer_)));
  }

  void OnUpdateAvailable(const std::string& app_id,
                         const std::u16string& app_name,
                         const base::Version& version) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(observer_);
    PostClosure(base::BindOnce(&AppInstallProgress::OnUpdateAvailable,
                               base::Unretained(observer_), app_id, app_name,
                               version));
  }

  void OnWaitingToDownload(const std::string& app_id,
                           const std::u16string& app_name) override {
  }

  void OnDownloading(const std::string& app_id,
                     const std::u16string& app_name,
                     std::optional<base::TimeDelta> time_remaining,
                     int pos) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(observer_);
    PostClosure(base::BindOnce(&AppInstallProgress::OnDownloading,
                               base::Unretained(observer_), app_id, app_name,
                               time_remaining, pos));
  }

  void OnWaitingRetryDownload(const std::string& app_id,
                              const std::u16string& app_name,
                              base::Time next_retry_time) override {
  }

  void OnWaitingToInstall(const std::string& app_id,
                          const std::u16string& app_name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(observer_);
    PostClosure(base::BindOnce(&AppInstallProgress::OnWaitingToInstall,
                               base::Unretained(observer_), app_id, app_name));
  }

  void OnInstalling(const std::string& app_id,
                    const std::u16string& app_name,
                    std::optional<base::TimeDelta> time_remaining,
                    int pos) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(observer_);
    PostClosure(base::BindOnce(&AppInstallProgress::OnInstalling,
                               base::Unretained(observer_), app_id, app_name,
                               time_remaining, pos));
  }

  void OnPause() override {}

  void OnComplete(const ObserverCompletionInfo& observer_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(observer_);
    PostClosure(base::BindOnce(&AppInstallProgress::OnComplete,
                               base::Unretained(observer_), observer_info));
  }

 private:
  void PostClosure(base::OnceClosure closure) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::unique_ptr<base::OnceClosure> closure_wrapper =
        std::make_unique<base::OnceClosure>(std::move(closure));
    ::PostThreadMessage(observer_thread_id_, WM_PROGRESS_WINDOW_IPC, 0,
                        reinterpret_cast<LPARAM>(closure_wrapper.release()));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  // This member is not owned by this class.
  raw_ptr<AppInstallProgress> observer_ = nullptr;

  // The thread id of the thread which creates the `observer_`. The thread
  // must have a message queue to enable this IPC class to post messages to it.
  DWORD observer_thread_id_ = 0;
};

void SetUsageStats(UpdaterScope scope,
                   const std::string& app_id,
                   std::optional<bool> usage_stats) {
  if (!usage_stats) {
    return;
  }

  const LONG result =
      base::win::RegKey(
          UpdaterScopeToHKeyRoot(scope),
          base::StrCat({CLIENT_STATE_KEY, base::UTF8ToWide(app_id)}).c_str(),
          Wow6432(KEY_WRITE))
          .WriteValue(L"usagestats", usage_stats.value() ? 1 : 0);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "Error writing usage stats for " << app_id << ":" << result;
    return;
  }

  if (IsSystemInstall(scope)) {
    base::win::RegKey(
        UpdaterScopeToHKeyRoot(scope),
        base::StrCat({CLIENT_STATE_MEDIUM_KEY, base::UTF8ToWide(app_id)})
            .c_str(),
        Wow6432(KEY_WRITE))
        .DeleteValue(L"usagestats");
  }
}

// Implements installing a single application by invoking the code in
// |UpdateService|, listening to |UpdateService| and UI events, and
// driving the UI code by calling the functions exposed by
// |AppInstallProgress|. This class receives state changes for an install
// and it notifies the UI, which is an observer of this class.
//
// The UI code can't run in a thread where the message loop is an instance of
// |base::MessageLoop|. |base::MessageLoop| does not handle all the messages
// needed by the UI, since the UI is written in terms of WTL, and it requires
// a |WTL::MessageLoop| to work, for example, accelerators, dialog messages,
// TAB key, etc are all handled by WTL. Therefore, the UI code runs on its own
// thread. This thread owns all the UI objects, which must be created and
// destroyed on this thread. The rest of the code in this class runs on
// the updater main thread.
//
// This class controls the lifetime of the UI thread. Once the UI thread is
// created, it is going to run a message loop until the main thread initiates
// its teardown by posting a WM_QUIT message to it. This makes the UI message
// loop exit, and after that, the execution flow returns to the scheduler task,
// which has been running the UI message loop. Upon its completion, the task
// posts a reply to the main thread, which makes the main thread exit its run
// loop, and then the main thread returns to the destructor of this class,
// and destructs its class members.
class AppInstallControllerImpl : public AppInstallController,
                                 public ui::ProgressWndEvents,
                                 public WTL::CMessageFilter {
 public:
  explicit AppInstallControllerImpl(bool is_silent_install);
  AppInstallControllerImpl();

  AppInstallControllerImpl(const AppInstallControllerImpl&) = delete;
  AppInstallControllerImpl& operator=(const AppInstallControllerImpl&) = delete;

  // Override for AppInstallController.
  void Initialize() override;

  void InstallApp(const std::string& app_id,
                  const std::string& app_name,
                  base::OnceCallback<void(int)> callback) override;

  void InstallAppOffline(const std::string& app_id,
                         const std::string& app_name,
                         base::OnceCallback<void(int)> callback) override;
  void Exit(int exit_code) override;

  void set_update_service(
      scoped_refptr<UpdateService> update_service) override {
    update_service_ = update_service;
  }

 private:
  friend class base::RefCountedThreadSafe<AppInstallControllerImpl>;

  ~AppInstallControllerImpl() override;

  // Overrides for OmahaWndEvents. These functions are called on the UI thread.
  void DoClose() override {}
  void DoExit() override;

  // Overrides for CompleteWndEvents. This function is called on the UI thread.
  bool DoLaunchBrowser(const std::string& url) override;

  // Overrides for ProgressWndEvents. These functions are called on the UI
  // thread.
  bool DoRestartBrowser(bool restart_all_browsers,
                        const std::vector<GURL>& urls) override;
  bool DoReboot() override;
  void DoCancel() override;

  // Overrides for WTL::CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  // This function is called on a dedicated COM STA thread.
  void LoadLogo(const std::string& app_id, HWND progress_hwnd);

  // These functions are called on the UI thread.
  void InitializeUI();
  void RunUI();

  // These functions are called on the main updater sequence.
  void PreInstallApp(const std::string& app_id,
                     const std::string& app_name,
                     base::OnceCallback<void(int)> callback);
  void DoInstallAppOffline(
      const update_client::ProtocolParser::Results& results,
      const std::string& installer_version,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data);
  void HandleOsNotSupported();
  void InstallComplete(UpdateService::Result result);

  // Returns the thread id of the thread which owns the progress window.
  DWORD GetUIThreadID() const;

  // Receives the state changes during handling of the Install function call.
  void StateChange(const UpdateService::UpdateState& update_state);

  SEQUENCE_CHECKER(sequence_checker_);

  // Provides an execution environment for the updater main sequence.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Provides an execution environment for the UI code. Typically, it runs
  // a single task which is the UI run loop.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // The application ID and the associated application name. The application
  // name is displayed by the UI and it must be localized.
  std::string app_id_;
  std::u16string app_name_;

  // The out-of-process service used for making RPC calls to install the app.
  scoped_refptr<UpdateService> update_service_;

  // The message loop associated with the UI.
  std::unique_ptr<WTL::CMessageLoop> ui_message_loop_;

  std::unique_ptr<AppInstallProgress> observer_;
  HWND observer_hwnd_ = nullptr;
  DWORD ui_thread_id_ = 0u;

  // The adapter for the inter-thread calls between the updater main thread
  // and the UI thread.
  std::unique_ptr<AppInstallProgressIPC> install_progress_observer_ipc_;

  // Contains the result of installing the application. This is populated by the
  // state change callback or the completion callback, if the former callback
  // was not posted.
  std::optional<ObserverCompletionInfo> observer_completion_info_;

  // Called when InstallApp is done.
  base::OnceCallback<void(int)> callback_;

  const bool is_silent_install_ = false;

  ProgressSampler download_progress_sampler_;
  ProgressSampler install_progress_sampler_;
};

AppInstallControllerImpl::AppInstallControllerImpl(bool is_silent_install)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      ui_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      is_silent_install_(is_silent_install),
      download_progress_sampler_(base::Seconds(5), base::Seconds(1)),
      install_progress_sampler_(base::Seconds(5), base::Seconds(1)) {}
AppInstallControllerImpl::~AppInstallControllerImpl() = default;

void AppInstallControllerImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::WaitableEvent ui_initialized_event;
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AppInstallControllerImpl> self,
             base::WaitableEvent& event) {
            self->InitializeUI();
            event.Signal();
          },
          base::WrapRefCounted(this), std::ref(ui_initialized_event)));

  ui_initialized_event.Wait();

  // The UI thread runs the observer.
  install_progress_observer_ipc_ =
      std::make_unique<AppInstallProgressIPC>(observer_.get(), ui_thread_id_);

  // At this point, the UI has been initialized, which means the UI
  // can be used from now on as an observer of the application
  // install. The task below runs the UI message loop for the UI until
  // it exits when a WM_QUIT message has been posted to it.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppInstallControllerImpl::RunUI, this));
}

void AppInstallControllerImpl::InstallApp(
    const std::string& app_id,
    const std::string& app_name,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PreInstallApp(app_id, app_name, std::move(callback));

  RegistrationRequest request;
  request.app_id = app_id_;
  request.version = base::Version(kNullVersion);
  std::optional<tagging::AppArgs> app_args = GetAppArgs(app_id_);
  std::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
  if (app_args) {
    request.ap = app_args->ap;
  }
  if (tag_args) {
    request.brand_code = tag_args->brand_code;
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&SetUsageStats, GetUpdaterScope(), app_id_,
                     tag_args ? tag_args->usage_stats_enable : std::nullopt),
      base::BindOnce(
          &UpdateService::Install, update_service_, request,
          GetDecodedInstallDataFromAppArgs(app_id_),
          GetInstallDataIndexFromAppArgs(app_id_),
          UpdateService::Priority::kForeground,
          base::BindRepeating(&AppInstallControllerImpl::StateChange, this),
          base::BindOnce(&AppInstallControllerImpl::InstallComplete, this)));
}

void AppInstallControllerImpl::PreInstallApp(
    const std::string& app_id,
    const std::string& app_name,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  app_id_ = app_id;
  app_name_ = base::UTF8ToUTF16(app_name);
  callback_ = std::move(callback);

  // The app logo is expected to be hosted at `{AppLogoURL}{url escaped
  // app_id_}.bmp`. If `{url escaped app_id_}.bmp` exists, a logo is shown in
  // the updater UI for that app install.
  //
  // For example, if `app_id_` is `{8A69D345-D564-463C-AFF1-A69D9E530F96}`,
  // the `{url escaped app_id_}.bmp` is
  // `%7b8A69D345-D564-463C-AFF1-A69D9E530F96%7d.bmp`.
  //
  // `AppLogoURL` is specified in external constants.
  base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
      ->PostTask(FROM_HERE, base::BindOnce(&AppInstallControllerImpl::LoadLogo,
                                           this, app_id_, observer_hwnd_));
}

void AppInstallControllerImpl::InstallAppOffline(
    const std::string& app_id,
    const std::string& app_name,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PreInstallApp(app_id, app_name, std::move(callback));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const std::string& app_id) {
            // Parse the offline manifest to get the install
            // command and install data.
            update_client::ProtocolParser::Results results;
            std::string installer_version;
            base::FilePath installer_path;
            std::string install_args;
            std::string install_data;
            ReadInstallCommandFromManifest(
                base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
                    kOfflineDirSwitch),
                app_id, GetInstallDataIndexFromAppArgs(app_id), results,
                installer_version, installer_path, install_args, install_data);

            const std::string client_install_data =
                GetDecodedInstallDataFromAppArgs(app_id);
            return std::make_tuple(
                results, installer_version, installer_path, install_args,
                client_install_data.empty() ? install_data
                                            : client_install_data);
          },
          app_id_),
      base::BindOnce(
          [](scoped_refptr<AppInstallControllerImpl> self,
             const std::tuple<
                 update_client::ProtocolParser::Results /*results*/,
                 std::string /*installer_version*/,
                 base::FilePath /*installer_path*/, std::string /*arguments*/,
                 std::string /*install_data*/>& result) {
            self->DoInstallAppOffline(std::get<0>(result), std::get<1>(result),
                                      std::get<2>(result), std::get<3>(result),
                                      std::get<4>(result));
          },
          base::WrapRefCounted(this)));
}

void AppInstallControllerImpl::DoInstallAppOffline(
    const update_client::ProtocolParser::Results& results,
    const std::string& installer_version,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsOsSupported(results)) {
    HandleOsNotSupported();
    return;
  }

  base::Value::Dict install_settings_dict;
  install_settings_dict.Set(kInstallerVersion, installer_version);

  const base::CommandLine cmd_line(*base::CommandLine::ForCurrentProcess());
  install_settings_dict.Set(kEnterpriseSwitch,
                            cmd_line.HasSwitch(kEnterpriseSwitch));
  install_settings_dict.Set(kSessionIdSwitch,
                            cmd_line.GetSwitchValueASCII(kSessionIdSwitch));

  std::string install_settings;
  if (!JSONStringValueSerializer(&install_settings)
           .Serialize(install_settings_dict)) {
    VLOG(1) << "Failed to serialize install settings.";
  }

  std::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
  RegistrationRequest request;
  request.app_id = app_id_;
  request.version = base::Version(kNullVersion);

  std::optional<tagging::AppArgs> app_args = GetAppArgs(app_id_);
  if (app_args) {
    request.ap = app_args->ap;
  }
  if (tag_args) {
    request.brand_code = tag_args->brand_code;
  }

  VLOG(1) << __func__ << ": " << installer_path << ": " << install_args << ": "
          << install_data;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&SetUsageStats, GetUpdaterScope(), app_id_,
                     tag_args ? tag_args->usage_stats_enable : std::nullopt),
      base::BindOnce(
          &UpdateService::RegisterApp, update_service_, request,
          base::BindOnce(
              [](scoped_refptr<AppInstallControllerImpl> self,
                 const base::FilePath& installer_path,
                 const std::string& install_args,
                 const std::string& install_data,
                 const std::string& install_settings, int result) {
                if (result != kRegistrationSuccess) {
                  VLOG(1) << "Registration failed: " << result;
                  self->InstallComplete(UpdateService::Result::kServiceFailed);
                  return;
                }
                self->update_service_->RunInstaller(
                    self->app_id_, installer_path, install_args, install_data,
                    install_settings,
                    base::BindRepeating(&AppInstallControllerImpl::StateChange,
                                        self),
                    base::BindOnce(&AppInstallControllerImpl::InstallComplete,
                                   self));
              },
              base::WrapRefCounted(this), installer_path, install_args,
              install_data, install_settings)));
}

void AppInstallControllerImpl::HandleOsNotSupported() {
  UpdateService::UpdateState update_state;
  update_state.app_id = app_id_;
  update_state.state = UpdateService::UpdateState::State::kUpdateError;
  update_state.error_category = UpdateService::ErrorCategory::kInstall;
  observer_completion_info_ = HandleInstallResult(update_state);
  observer_completion_info_->completion_text =
      base::WideToUTF16(GetLocalizedString(IDS_UPDATER_OS_NOT_SUPPORTED_BASE));
  InstallComplete(UpdateService::Result::kInstallFailed);
}

void AppInstallControllerImpl::InstallComplete(UpdateService::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  // Create a best-effort `UpdateState` instance if one is not available because
  // state change callbacks were never posted for this install. This happens if
  // the execution path returns early, before it has reached the state machine
  // of the component in the `update_client`.
  if (!observer_completion_info_.has_value()) {
    UpdateService::UpdateState update_state;
    update_state.app_id = app_id_;
    update_state.state = UpdateService::UpdateState::State::kUpdateError;
    update_state.error_code = static_cast<int>(result);
    update_state.error_category = [result] {
      switch (result) {
        case UpdateService::Result::kUpdateCheckFailed:
          return UpdateService::ErrorCategory::kUpdateCheck;
        case UpdateService::Result::kInstallFailed:
          return UpdateService::ErrorCategory::kInstall;
        default:
          return UpdateService::ErrorCategory::kService;
      }
    }();
    observer_completion_info_ = HandleInstallResult(update_state);
  }
  update_service_ = nullptr;
  CHECK(observer_completion_info_.has_value());
  install_progress_observer_ipc_->OnComplete(observer_completion_info_.value());
}

void AppInstallControllerImpl::Exit(int exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  update_service_ = nullptr;

  if (exit_code == kErrorOk) {
    install_progress_observer_ipc_->OnComplete({});
  }

  UpdateService::UpdateState update_state;
  update_state.state = UpdateService::UpdateState::State::kNotStarted;
  update_state.error_code = exit_code;
  install_progress_observer_ipc_->OnComplete(HandleInstallResult(update_state));
}
void AppInstallControllerImpl::StateChange(
    const UpdateService::UpdateState& update_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AppInstallProgressIPC* dbg_ipc = install_progress_observer_ipc_.get();
  base::debug::Alias(&dbg_ipc);
  CHECK(install_progress_observer_ipc_);

  // TODO(crbug.com/345250525) - understand why the check fails.
  UpdateService::UpdateState::State state = update_state.state;
  base::debug::Alias(&state);
  DEBUG_ALIAS_FOR_CSTR(dbg_app_id1, app_id_.c_str(), 64);
  DEBUG_ALIAS_FOR_CSTR(dbg_app_id2, update_state.app_id.c_str(), 64);
  if (app_id_ != update_state.app_id) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  switch (update_state.state) {
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      install_progress_observer_ipc_->OnCheckingForUpdate();
      break;

    case UpdateService::UpdateState::State::kUpdateAvailable:
      install_progress_observer_ipc_->OnUpdateAvailable(
          app_id_, app_name_, update_state.next_version);
      break;

    case UpdateService::UpdateState::State::kDownloading: {
      const auto pos = GetDownloadProgress(update_state.downloaded_bytes,
                                           update_state.total_bytes);
      if (pos >= 0) {
        download_progress_sampler_.AddSample(update_state.downloaded_bytes);
      }
      install_progress_observer_ipc_->OnDownloading(
          app_id_, app_name_,
          download_progress_sampler_.GetRemainingTime(update_state.total_bytes),
          pos >= 0 ? pos : 0);
      break;
    }

    case UpdateService::UpdateState::State::kInstalling: {
      install_progress_observer_ipc_->OnWaitingToInstall(app_id_, app_name_);
      const int pos = update_state.install_progress;  // [0..100]
      if (pos >= 0) {
        install_progress_sampler_.AddSample(pos);
      }
      install_progress_observer_ipc_->OnInstalling(
          app_id_, app_name_, install_progress_sampler_.GetRemainingTime(100),
          pos >= 0 ? pos : 0);
      break;
    }

    case UpdateService::UpdateState::State::kUpdated:
    case UpdateService::UpdateState::State::kNoUpdate:
    case UpdateService::UpdateState::State::kUpdateError:
      observer_completion_info_ = HandleInstallResult(update_state);
      break;

    case UpdateService::UpdateState::State::kUnknown:
    case UpdateService::UpdateState::State::kNotStarted:
      break;
  }
}

// Loads the logo in BMP format if it exists for the provided `app_id`, and sets
// the resultant image onto the app bitmap for the progress window.
void AppInstallControllerImpl::LoadLogo(const std::string& app_id,
                                        HWND progress_hwnd) {
  std::wstring url = base::UTF8ToWide(base::StringPrintf(
      "%s%s.bmp?lang=%s",
      CreateExternalConstants()->AppLogoURL().possibly_invalid_spec().c_str(),
      base::EscapeUrlEncodedData(app_id, false).c_str(),
      base::WideToUTF8(GetPreferredLanguage()).c_str()));
  if (url.empty()) {
    VLOG(1) << __func__ << "No url specified";
    return;
  }

  Microsoft::WRL::ComPtr<IPicture> picture;
  HRESULT hr =
      ::OleLoadPicturePath(&url[0], nullptr, 0, 0, IID_PPV_ARGS(&picture));
  if (FAILED(hr)) {
    VLOG(1) << __func__ << "::OleLoadPicturePath failed: " << url << ": "
            << std::hex << hr << ": " << logging::SystemErrorCodeToString(hr);
    return;
  }

  HBITMAP bitmap = nullptr;
  hr = picture->get_Handle(reinterpret_cast<UINT*>(&bitmap));
  if (FAILED(hr)) {
    VLOG(1) << __func__ << "picture->get_Handle failed: " << std::hex << hr
            << ": " << logging::SystemErrorCodeToString(hr);
    return;
  }

  if (!::IsWindow(progress_hwnd)) {
    VLOG(1) << __func__ << "progress_hwnd not valid anymore";
    return;
  }

  ::SendDlgItemMessage(progress_hwnd, IDC_APP_BITMAP, STM_SETIMAGE,
                       IMAGE_BITMAP,
                       reinterpret_cast<LPARAM>(::CopyImage(
                           bitmap, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG)));
}

// Creates the install progress observer. The observer has thread affinity. It
// must be created, process its messages, and be destroyed on the same thread.
void AppInstallControllerImpl::InitializeUI() {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  base::ScopedDisallowBlocking no_blocking_allowed_on_ui_thread;

  ui_message_loop_ = std::make_unique<WTL::CMessageLoop>();
  ui_message_loop_->AddMessageFilter(this);
  ui_thread_id_ = ::GetCurrentThreadId();

  if (is_silent_install_) {
    observer_ = std::make_unique<InstallProgressSilentObserver>(this);
  } else {
    auto progress_wnd =
        std::make_unique<ui::ProgressWnd>(ui_message_loop_.get(), nullptr);

    std::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
    if (tag_args) {
      progress_wnd->set_bundle_name(base::UTF8ToUTF16(tag_args->bundle_name));
    }
    progress_wnd->SetEventSink(this);
    progress_wnd->Initialize();
    progress_wnd->Show();

    observer_hwnd_ = progress_wnd->m_hWnd;
    observer_.reset(progress_wnd.release());
  }
}

void AppInstallControllerImpl::RunUI() {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  CHECK_EQ(GetUIThreadID(), GetCurrentThreadId());

  ui_message_loop_->Run();
  ui_message_loop_->RemoveMessageFilter(this);

  // This object is owned by the UI thread must be destroyed on this thread.
  observer_ = nullptr;

  if (!callback_) {
    return;
  }
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback_), kErrorOk));
}

void AppInstallControllerImpl::DoExit() {
  CHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
}

BOOL AppInstallControllerImpl::PreTranslateMessage(MSG* msg) {
  if (const auto ui_thread_id = GetUIThreadID(); ui_thread_id != 0) {
    CHECK_EQ(ui_thread_id, GetCurrentThreadId());
  } else {
    VLOG(1) << "Can't find a thread id for the message: " << msg->message;
  }
  if (msg->message == AppInstallProgressIPC::WM_PROGRESS_WINDOW_IPC) {
    install_progress_observer_ipc_->Invoke(msg->wParam, msg->lParam);
    return true;
  }
  return false;
}

DWORD AppInstallControllerImpl::GetUIThreadID() const {
  CHECK_NE(ui_thread_id_, 0u);
  return ui_thread_id_;
}

bool AppInstallControllerImpl::DoLaunchBrowser(const std::string& url) {
  CHECK_EQ(GetUIThreadID(), GetCurrentThreadId());

  return SUCCEEDED(base::win::RunDeElevatedNoWait(base::UTF8ToWide(url), {}));
}

bool AppInstallControllerImpl::DoRestartBrowser(bool restart_all_browsers,
                                                const std::vector<GURL>& urls) {
  CHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  return false;
}

bool AppInstallControllerImpl::DoReboot() {
  CHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  return false;
}

void AppInstallControllerImpl::DoCancel() {
  CHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  if (!update_service_) {
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateService::CancelInstalls, update_service_, app_id_));
}

std::wstring GetTextForStartupError(int error_code) {
  switch (error_code) {
    case kErrorWrongUser:
      return GetLocalizedString(
          ::IsUserAnAdmin() ? IDS_WRONG_USER_DEELEVATION_REQUIRED_ERROR_BASE
                            : IDS_WRONG_USER_ELEVATION_REQUIRED_ERROR_BASE);
    case kErrorFailedToLockSetupMutex:
      return GetLocalizedString(IDS_UNABLE_TO_GET_SETUP_LOCK_BASE);
    default:
      return GetLocalizedStringF(IDS_GENERIC_STARTUP_ERROR_BASE,
                                 GetTextForSystemError(error_code));
  }
}

}  // namespace

std::string GetInstallerText(UpdateService::ErrorCategory error_category,
                             int error_code,
                             int extra_code) {
  if (!error_code) {
    return {};
  }
  std::string value = base::WideToUTF8(base::StrCat(
      {[&] {
         switch (error_category) {
           case UpdateService::ErrorCategory::kInstall:
             return GetTextForUpdateClientInstallError(error_code);
           case UpdateService::ErrorCategory::kDownload:
             return GetTextForDownloadError(error_code);
           case UpdateService::ErrorCategory::kUnpack:
             return GetTextForUnpackError(error_code);
           case UpdateService::ErrorCategory::kService:
             return GetTextForServiceError(error_code);
           case UpdateService::ErrorCategory::kUpdateCheck:
             return GetTextForUpdateCheckError(error_code);
           case UpdateService::ErrorCategory::kInstaller:
             return GetTextForInstallerError(error_code);
           default:
             LOG(ERROR) << "Unknown error category: " << error_category;
             return std::wstring();
         }
       }(),
       [&] {
         if (!extra_code) {
           return std::wstring();
         }
         return base::StrCat(
             {L"\n", GetLocalizedStringF(IDS_EXTRA_CODE_BASE,
                                         base::ASCIIToWide(base::StringPrintf(
                                             "%#x", extra_code)))});
       }()}));
  VLOG(1) << __func__ << ": " << value;
  return value;
}

[[nodiscard]] ObserverCompletionInfo HandleInstallResult(
    const UpdateService::UpdateState& update_state) {
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
  std::wstring completion_text;
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kUpdated:
      VLOG(1) << "Update success.";
      completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
      completion_text =
          GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE);
      break;
    case UpdateService::UpdateState::State::kNoUpdate:
      VLOG(1) << "No updates.";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      completion_text = GetLocalizedString(IDS_NO_UPDATE_RESPONSE_BASE);
      break;
    case UpdateService::UpdateState::State::kUpdateError:
      VLOG(1) << "Updater error: " << update_state.error_code << ".";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      completion_text = GetLocalizedString(IDS_INSTALL_UPDATER_FAILED_BASE);
      break;
    case UpdateService::UpdateState::State::kNotStarted:
      VLOG(1) << "Updater error: " << update_state.error_code << ".";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      completion_text = GetTextForStartupError(update_state.error_code);
      break;
    default:
      NOTREACHED();
  }

  ObserverCompletionInfo observer_info;
  observer_info.completion_code = completion_code;
  observer_info.completion_text = base::WideToUTF16(completion_text);
  observer_info.help_url = GURL(base::StringPrintf(
      "%s?product=%s&error=%d", HELP_CENTER_URL,
      base::EscapeUrlEncodedData(update_state.app_id, false).c_str(),
      update_state.error_code));

  AppCompletionInfo app_info;
  if (update_state.state == UpdateService::UpdateState::State::kNotStarted) {
    app_info.app_id = kUpdaterAppId;
  } else if (update_state.state !=
             UpdateService::UpdateState::State::kNoUpdate) {
    app_info.app_id = update_state.app_id;
    app_info.error_code = update_state.error_code;
    app_info.completion_message =
        update_state.installer_text.empty()
            ? base::UTF8ToUTF16(GetInstallerText(update_state.error_category,
                                                 update_state.error_code,
                                                 update_state.extra_code1))
            : base::UTF8ToUTF16(update_state.installer_text);
    app_info.extra_code1 = update_state.extra_code1;
    app_info.post_install_launch_command_line = update_state.installer_cmd_line;
    VLOG(1) << app_info.app_id << " installation completed: error category["
            << update_state.error_category << "], error_code["
            << app_info.error_code << "], extra_code1[" << app_info.extra_code1
            << "], completion_message[" << app_info.completion_message
            << "], post_install_launch_command_line["
            << app_info.post_install_launch_command_line << "]";

    // If the installer provides a launch command,
    // `COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND` will cause the UI
    // client to run the launch command and exit in the interactive install
    // case.
    if (app_info.error_code == 0) {
      app_info.completion_code =
          app_info.post_install_launch_command_line.empty()
              ? CompletionCodes::COMPLETION_CODE_SUCCESS
              : CompletionCodes::
                    COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND;
    } else if (app_info.error_code == ERROR_SUCCESS_REBOOT_INITIATED ||
               app_info.error_code == ERROR_SUCCESS_REBOOT_REQUIRED ||
               app_info.error_code == ERROR_SUCCESS_RESTART_REQUIRED) {
      app_info.completion_code =
          CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY;
    } else {
      app_info.completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
    }
  }
  observer_info.apps_info.push_back(app_info);

  return observer_info;
}

scoped_refptr<App> MakeAppInstall(bool is_silent_install) {
  if (IsSystemInstall()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(kOemSwitch)) {
      const bool success = SetOemInstallState();
      LOG_IF(ERROR, !success) << "SetOemInstallState failed";
    }

    std::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
    if (tag_args && !tag_args->enrollment_token.empty()) {
      const bool success =
          StoreRunTimeEnrollmentToken(tag_args->enrollment_token);
      LOG_IF(ERROR, !success) << "StoreRunTimeEnrollmentToken failed";
    }
  }
  return base::MakeRefCounted<AppInstall>(base::BindRepeating(
      [](bool is_silent_install) -> scoped_refptr<AppInstallController> {
        return base::MakeRefCounted<AppInstallControllerImpl>(
            is_silent_install);
      },
      is_silent_install));
}

}  // namespace updater
