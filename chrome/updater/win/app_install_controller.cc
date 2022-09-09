// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/updater/app/app_install.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <shldisp.h>
#include <shlobj.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/shlwapi.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/install_progress_observer.h"
#include "chrome/updater/win/manifest_util.h"
#include "chrome/updater/win/scoped_impersonation.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/splash_screen.h"
#include "chrome/updater/win/ui/ui_util.h"
#pragma clang diagnostic pop

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

bool GetShellDispatch(Microsoft::WRL::ComPtr<IShellDispatch2>* shell_dispatch) {
  long hwnd = 0;
  Microsoft::WRL::ComPtr<IShellWindows> shell;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  Microsoft::WRL::ComPtr<IServiceProvider> service;
  Microsoft::WRL::ComPtr<IShellBrowser> browser;
  Microsoft::WRL::ComPtr<IShellView> view;
  Microsoft::WRL::ComPtr<IShellFolderViewDual> folder;
  return SUCCEEDED(::CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL,
                                      IID_PPV_ARGS(&shell))) &&
         SUCCEEDED(shell->FindWindowSW(
             base::win::ScopedVariant(CSIDL_DESKTOP).AsInput(), nullptr,
             SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &dispatch)) &&
         SUCCEEDED(dispatch.As(&service)) &&
         SUCCEEDED(service->QueryService(SID_STopLevelBrowser,
                                         IID_PPV_ARGS(&browser))) &&
         SUCCEEDED(browser->QueryActiveShellView(&view)) &&
         SUCCEEDED(
             view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&dispatch))) &&
         SUCCEEDED(dispatch.As(&folder)) &&
         SUCCEEDED(folder->get_Application(&dispatch)) &&
         SUCCEEDED(dispatch.As(shell_dispatch));
}

class InstallProgressSilentObserver : public InstallProgressObserver {
 public:
  explicit InstallProgressSilentObserver(ui::OmahaWndEvents* events_sink);
  ~InstallProgressSilentObserver() override = default;

  // Overrides for InstallProgressObserver.
  // These functions are called on the thread which owns this class.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const std::u16string& app_id,
                         const std::u16string& app_name,
                         const std::u16string& version_string) override;
  void OnWaitingToDownload(const std::u16string& app_id,
                           const std::u16string& app_name) override;
  void OnDownloading(const std::u16string& app_id,
                     const std::u16string& app_name,
                     int time_remaining_ms,
                     int pos) override;
  void OnWaitingRetryDownload(const std::u16string& app_id,
                              const std::u16string& app_name,
                              const base::Time& next_retry_time) override;
  void OnWaitingToInstall(const std::u16string& app_id,
                          const std::u16string& app_name,
                          bool* can_start_install) override;
  void OnInstalling(const std::u16string& app_id,
                    const std::u16string& app_name,
                    int time_remaining_ms,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

 private:
  THREAD_CHECKER(thread_checker_);

  // Event sink must out-live this observer.
  raw_ptr<ui::OmahaWndEvents> events_sink_ = nullptr;
};

InstallProgressSilentObserver::InstallProgressSilentObserver(
    ui::OmahaWndEvents* events_sink)
    : events_sink_(events_sink) {
  DCHECK(events_sink_);
}

void InstallProgressSilentObserver::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnUpdateAvailable(
    const std::u16string& app_id,
    const std::u16string& app_name,
    const std::u16string& version_string) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnWaitingToDownload(
    const std::u16string& app_id,
    const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnDownloading(
    const std::u16string& app_id,
    const std::u16string& app_name,
    int time_remaining_ms,
    int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnWaitingRetryDownload(
    const std::u16string& app_id,
    const std::u16string& app_name,
    const base::Time& next_retry_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnWaitingToInstall(
    const std::u16string& app_id,
    const std::u16string& app_name,
    bool* can_start_install) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnInstalling(const std::u16string& app_id,
                                                 const std::u16string& app_name,
                                                 int time_remaining_ms,
                                                 int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnPause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressSilentObserver::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(events_sink_);
  VLOG(1) << __func__;

  // TODO(crbug.com/1286580): Launch `post_install_launch_command_line` for
  // each app if needed.

  events_sink_->DoExit();
}

// Implements a simple inter-thread communication protocol based on Windows
// messages exchanged between the application installer and its UI.
//
// Since the installer code and the UI code execute on different threads, the
// installer can't invoke directly functions exposed by the UI.
// This class translates a function call made by the installer to any of the
// overriden virtual functions into posting a window message with specific
// arguments to the progress window implemented by the UI. Handling such
// message consists of unpacking the arguments of the message, and invoking
// the UI code in the correct thread, which is the thread that owns the window.
class InstallProgressObserverIPC : public InstallProgressObserver {
 public:
  // Used as an inter-thread communication mechanism between the installer and
  // UI threads.
  static constexpr unsigned int WM_PROGRESS_WINDOW_IPC = WM_APP + 1;

  InstallProgressObserverIPC(InstallProgressObserver* observer,
                             DWORD observer_thread_id);
  InstallProgressObserverIPC(const InstallProgressObserverIPC&) = delete;
  InstallProgressObserverIPC& operator=(const InstallProgressObserverIPC&) =
      delete;
  ~InstallProgressObserverIPC() override = default;

  // Called by the window proc when a specific application message is processed
  // by the progress window. This call always occurs in the context of the
  // thread which owns the window.
  void Invoke(WPARAM wparam, LPARAM lparam);

  // Overrides for InstallProgressObserver.
  // Called by the application installer code on the main udpater thread.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const std::u16string& app_id,
                         const std::u16string& app_name,
                         const std::u16string& version_string) override;
  void OnWaitingToDownload(const std::u16string& app_id,
                           const std::u16string& app_name) override;
  void OnDownloading(const std::u16string& app_id,
                     const std::u16string& app_name,
                     int time_remaining_ms,
                     int pos) override;
  void OnWaitingRetryDownload(const std::u16string& app_id,
                              const std::u16string& app_name,
                              const base::Time& next_retry_time) override;
  void OnWaitingToInstall(const std::u16string& app_id,
                          const std::u16string& app_name,
                          bool* can_start_install) override;
  void OnInstalling(const std::u16string& app_id,
                    const std::u16string& app_name,
                    int time_remaining_ms,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

 private:
  enum class IPCAppMessages {
    kOnCheckingForUpdate = 0,
    kOnUpdateAvailable,
    kOnWaitingToDownload,
    kOnDownloading,
    kOnWaitingRetryDownload,
    kOnWaitingToInstall,
    kOnInstalling,
    kOnPause,
    kOnComplete,
  };

  struct ParamOnUpdateAvailable {
    ParamOnUpdateAvailable() = default;
    ParamOnUpdateAvailable(const ParamOnUpdateAvailable&) = delete;
    ParamOnUpdateAvailable& operator=(const ParamOnUpdateAvailable&) = delete;

    std::u16string app_id;
    std::u16string app_name;
    std::u16string version_string;
  };

  struct ParamOnDownloading {
    ParamOnDownloading() = default;
    ParamOnDownloading(const ParamOnDownloading&) = delete;
    ParamOnDownloading& operator=(const ParamOnDownloading&) = delete;

    std::u16string app_id;
    std::u16string app_name;
    int time_remaining_ms = 0;
    int pos = 0;
  };

  struct ParamOnWaitingToInstall {
    ParamOnWaitingToInstall() = default;
    ParamOnWaitingToInstall(const ParamOnWaitingToInstall&) = delete;
    ParamOnWaitingToInstall& operator=(const ParamOnWaitingToInstall&) = delete;

    std::u16string app_id;
    std::u16string app_name;
  };

  struct ParamOnInstalling {
    ParamOnInstalling() = default;
    ParamOnInstalling(const ParamOnInstalling&) = delete;
    ParamOnInstalling& operator=(const ParamOnInstalling&) = delete;

    std::u16string app_id;
    std::u16string app_name;
    int time_remaining_ms = 0;
    int pos = 0;
  };

  struct ParamOnComplete {
    ParamOnComplete() = default;
    ParamOnComplete(const ParamOnComplete&) = delete;
    ParamOnComplete& operator=(const ParamOnComplete&) = delete;

    ObserverCompletionInfo observer_info;
  };

  THREAD_CHECKER(thread_checker_);

  // This member is not owned by this class.
  raw_ptr<InstallProgressObserver> observer_ = nullptr;

  // The thread id of the thread which creates the `observer_`. The thread
  // must have a message queue to enable this IPC class to post messages to it.
  DWORD observer_thread_id_ = 0;
};

InstallProgressObserverIPC::InstallProgressObserverIPC(
    InstallProgressObserver* observer,
    DWORD obserer_thread_id)
    : observer_(observer), observer_thread_id_(obserer_thread_id) {
  DCHECK(observer);
}

void InstallProgressObserverIPC::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_);
  ::PostThreadMessage(observer_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnCheckingForUpdate),
                      0);
}

void InstallProgressObserverIPC::OnUpdateAvailable(
    const std::u16string& app_id,
    const std::u16string& app_name,
    const std::u16string& version_string) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_);
  std::unique_ptr<ParamOnUpdateAvailable> param_on_update_available =
      std::make_unique<ParamOnUpdateAvailable>();
  param_on_update_available->app_id = app_id;
  param_on_update_available->app_name = app_name;
  param_on_update_available->version_string = version_string;
  ::PostThreadMessage(
      observer_thread_id_, WM_PROGRESS_WINDOW_IPC,
      static_cast<WPARAM>(IPCAppMessages::kOnUpdateAvailable),
      reinterpret_cast<LPARAM>(param_on_update_available.release()));
}

void InstallProgressObserverIPC::OnWaitingToDownload(
    const std::u16string& app_id,
    const std::u16string& app_name) {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnDownloading(const std::u16string& app_id,
                                               const std::u16string& app_name,
                                               int time_remaining_ms,
                                               int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_);
  std::unique_ptr<ParamOnDownloading> param_on_downloading =
      std::make_unique<ParamOnDownloading>();
  param_on_downloading->app_id = app_id;
  param_on_downloading->app_name = app_name;
  param_on_downloading->time_remaining_ms = time_remaining_ms;
  param_on_downloading->pos = pos;
  ::PostThreadMessage(observer_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnDownloading),
                      reinterpret_cast<LPARAM>(param_on_downloading.release()));
}

void InstallProgressObserverIPC::OnWaitingRetryDownload(
    const std::u16string& app_id,
    const std::u16string& app_name,
    const base::Time& next_retry_time) {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnWaitingToInstall(
    const std::u16string& app_id,
    const std::u16string& app_name,
    bool* can_start_install) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_);
  std::unique_ptr<ParamOnWaitingToInstall> param_on_waiting_to_install =
      std::make_unique<ParamOnWaitingToInstall>();
  param_on_waiting_to_install->app_id = app_id;
  param_on_waiting_to_install->app_name = app_name;
  ::PostThreadMessage(
      observer_thread_id_, WM_PROGRESS_WINDOW_IPC,
      static_cast<WPARAM>(IPCAppMessages::kOnWaitingToInstall),
      reinterpret_cast<LPARAM>(param_on_waiting_to_install.release()));
}

void InstallProgressObserverIPC::OnInstalling(const std::u16string& app_id,
                                              const std::u16string& app_name,
                                              int time_remaining_ms,
                                              int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(sorin): implement progress, https://crbug.com/1014594.
  DCHECK(observer_);
  std::unique_ptr<ParamOnInstalling> param_on_installing =
      std::make_unique<ParamOnInstalling>();
  param_on_installing->app_id = app_id;
  param_on_installing->app_name = app_name;
  param_on_installing->time_remaining_ms = time_remaining_ms;
  param_on_installing->pos = pos;
  ::PostThreadMessage(observer_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnInstalling),
                      reinterpret_cast<LPARAM>(param_on_installing.release()));
}

void InstallProgressObserverIPC::OnPause() {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_);
  std::unique_ptr<ParamOnComplete> param_on_complete =
      std::make_unique<ParamOnComplete>();
  param_on_complete->observer_info = observer_info;
  ::PostThreadMessage(observer_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnComplete),
                      reinterpret_cast<LPARAM>(param_on_complete.release()));
}

void InstallProgressObserverIPC::Invoke(WPARAM wparam, LPARAM lparam) {
  DCHECK_EQ(observer_thread_id_, ::GetCurrentThreadId());
  switch (static_cast<IPCAppMessages>(wparam)) {
    case IPCAppMessages::kOnCheckingForUpdate:
      observer_->OnCheckingForUpdate();
      break;
    case IPCAppMessages::kOnUpdateAvailable: {
      std::unique_ptr<ParamOnUpdateAvailable> param_on_update_available(
          reinterpret_cast<ParamOnUpdateAvailable*>(lparam));
      observer_->OnUpdateAvailable(param_on_update_available->app_id,
                                   param_on_update_available->app_name,
                                   param_on_update_available->version_string);
      break;
    }
    case IPCAppMessages::kOnDownloading: {
      std::unique_ptr<ParamOnDownloading> param_on_downloading(
          reinterpret_cast<ParamOnDownloading*>(lparam));
      observer_->OnDownloading(
          param_on_downloading->app_id, param_on_downloading->app_name,
          param_on_downloading->time_remaining_ms, param_on_downloading->pos);
      break;
    }
    case IPCAppMessages::kOnWaitingToInstall: {
      std::unique_ptr<ParamOnWaitingToInstall> param_on_waiting_to_install(
          reinterpret_cast<ParamOnWaitingToInstall*>(lparam));
      // TODO(sorin): implement cancelling of an install. crbug.com/1014591
      bool can_install = false;
      observer_->OnWaitingToInstall(param_on_waiting_to_install->app_id,
                                    param_on_waiting_to_install->app_name,
                                    &can_install);
      break;
    }
    case IPCAppMessages::kOnInstalling: {
      std::unique_ptr<ParamOnInstalling> param_on_installing(
          reinterpret_cast<ParamOnInstalling*>(lparam));
      observer_->OnInstalling(
          param_on_installing->app_id, param_on_installing->app_name,
          param_on_installing->time_remaining_ms, param_on_installing->pos);
      break;
    }
    case IPCAppMessages::kOnComplete: {
      std::unique_ptr<ParamOnComplete> param_on_complete(
          reinterpret_cast<ParamOnComplete*>(lparam));
      observer_->OnComplete(param_on_complete->observer_info);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void SetUsageStats(UpdaterScope scope,
                   const std::string& app_id,
                   bool usage_stats_enabled) {
  const LONG result =
      base::win::RegKey(
          scope == UpdaterScope::kUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
          base::StrCat({scope == UpdaterScope::kUser ? CLIENT_STATE_KEY
                                                     : CLIENT_STATE_MEDIUM_KEY,
                        base::SysUTF8ToWide(app_id)})
              .c_str(),
          Wow6432(KEY_WRITE))
          .WriteValue(L"usagestats", usage_stats_enabled ? 1 : 0);
  VLOG_IF(1, result != ERROR_SUCCESS)
      << "Error writing usage stats for " << app_id << ":" << result;
}

// Implements installing a single application by invoking the code in
// |UpdateService|, listening to |UpdateService| and UI events, and
// driving the UI code by calling the functions exposed by
// |InstallProgressObserver|. This class receives state changes for an install
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
  explicit AppInstallControllerImpl(
      bool is_silent_install,
      scoped_refptr<UpdateService> update_service);
  AppInstallControllerImpl();

  AppInstallControllerImpl(const AppInstallControllerImpl&) = delete;
  AppInstallControllerImpl& operator=(const AppInstallControllerImpl&) = delete;

  // Override for AppInstallController.
  void InstallApp(const std::string& app_id,
                  const std::string& app_name,
                  base::OnceCallback<void(int)> callback) override;

  void InstallAppOffline(const std::string& app_id,
                         const std::string& app_name,
                         const base::FilePath& offline_dir,
                         bool enterprise,
                         base::OnceCallback<void(int)> callback) override;

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
                        const std::vector<std::u16string>& urls) override;
  bool DoReboot() override;
  void DoCancel() override;

  // Overrides for WTL::CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  // These functions are called on the UI thread.
  void InitializeUI();
  void RunUI();

  // These functions are called on the main updater thread.
  void DoInstallApp();
  void DoInstallAppOffline(const base::FilePath& installer_path,
                           const std::string& install_args,
                           const std::string& install_data,
                           bool enterprise);
  void InstallComplete(UpdateService::Result result);
  void HandleInstallResult(const UpdateService::UpdateState& update_state);

  // Returns the thread id of the thread which owns the progress window.
  DWORD GetUIThreadID() const;

  // Receives the state changes during handling of the Install function call.
  void StateChange(const UpdateService::UpdateState& update_state);

  SEQUENCE_CHECKER(sequence_checker_);

  // Provides an execution environment for the updater main thread.
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

  std::unique_ptr<InstallProgressObserver> observer_;
  DWORD ui_thread_id_ = 0u;

  // The adapter for the inter-thread calls between the updater main thread
  // and the UI thread.
  std::unique_ptr<InstallProgressObserverIPC> install_progress_observer_ipc_;

  // Called when InstallApp is done.
  base::OnceCallback<void(int)> callback_;

  const bool is_silent_install_ = false;
};

// TODO(sorin): fix the hardcoding of the application name.
// https:crbug.com/1296931
AppInstallControllerImpl::AppInstallControllerImpl(
    bool is_silent_install,
    scoped_refptr<UpdateService> update_service)
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      ui_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      update_service_(update_service),
      is_silent_install_(is_silent_install) {}
AppInstallControllerImpl::~AppInstallControllerImpl() = default;

void AppInstallControllerImpl::InstallApp(
    const std::string& app_id,
    const std::string& app_name,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  app_id_ = app_id;
  app_name_ = base::UTF8ToUTF16(app_name);
  callback_ = std::move(callback);

  ui_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&AppInstallControllerImpl::InitializeUI, this),
      base::BindOnce(&AppInstallControllerImpl::DoInstallApp, this));
}

void AppInstallControllerImpl::DoInstallApp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // At this point, the UI has been initialized, which means the UI can be
  // used from now on as an observer of the application install. The task
  // below runs the UI message loop for the UI until it exits, because
  // a WM_QUIT message has been posted to it.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppInstallControllerImpl::RunUI, this));

  // The UI thread runs the observer.
  install_progress_observer_ipc_ = std::make_unique<InstallProgressObserverIPC>(
      observer_.get(), ui_thread_id_);

  RegistrationRequest request;
  request.app_id = app_id_;
  absl::optional<tagging::AppArgs> app_args = GetAppArgs(app_id_);
  absl::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
  if (app_args)
    request.ap = app_args->ap;
  if (tag_args)
    request.brand_code = tag_args->brand_code;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&SetUsageStats, GetUpdaterScope(), app_id_,
                     tag_args && tag_args->usage_stats_enable &&
                         *tag_args->usage_stats_enable),
      base::BindOnce(
          &UpdateService::Install, update_service_, request,
          GetInstallDataIndexFromAppArgs(app_id_),
          UpdateService::Priority::kForeground,
          base::BindRepeating(&AppInstallControllerImpl::StateChange, this),
          base::BindOnce(&AppInstallControllerImpl::InstallComplete, this)));
}

void AppInstallControllerImpl::InstallAppOffline(
    const std::string& app_id,
    const std::string& app_name,
    const base::FilePath& offline_dir,
    bool enterprise,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  app_id_ = app_id;
  app_name_ = base::UTF8ToUTF16(app_name);
  callback_ = std::move(callback);

  ui_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&AppInstallControllerImpl::InitializeUI, this),
      base::BindOnce(
          [](scoped_refptr<AppInstallControllerImpl> self,
             const base::FilePath& offline_dir, bool enterprise) {
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, {base::MayBlock()},
                base::BindOnce(
                    [](const base::FilePath& offline_dir,
                       const std::string& app_id) {
                      // Parse the offline manifest to get the install
                      // command and install data.
                      base::FilePath installer_path;
                      std::string install_args;
                      std::string install_data;

                      absl::optional<tagging::AppArgs> app_args =
                          GetAppArgs(app_id);
                      ReadInstallCommandFromManifest(
                          offline_dir, app_id,
                          app_args ? app_args->install_data_index
                                   : std::string(),
                          installer_path, install_args, install_data);
                      return std::make_tuple(installer_path, install_args,
                                             install_data);
                    },
                    offline_dir, self->app_id_),
                base::BindOnce(
                    [](scoped_refptr<AppInstallControllerImpl> self,
                       bool enterprise,
                       const std::tuple<base::FilePath /*installer_path*/,
                                        std::string /*arguments*/,
                                        std::string /*install_data*/>& result) {
                      self->DoInstallAppOffline(
                          std::get<0>(result), std::get<1>(result),
                          std::get<2>(result), enterprise);
                    },
                    self, enterprise));
          },
          base::WrapRefCounted(this), offline_dir, enterprise));
}

void AppInstallControllerImpl::DoInstallAppOffline(
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    bool enterprise) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // At this point, the UI has been initialized, which means the UI can be
  // used from now on as an observer of the application install. The task
  // below runs the UI message loop until it exits, because a WM_QUIT message
  // has been posted to it.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppInstallControllerImpl::RunUI, this));

  // The UI thread runs the observer.
  install_progress_observer_ipc_ = std::make_unique<InstallProgressObserverIPC>(
      observer_.get(), ui_thread_id_);

  // TODO(crbug.com/1286581): fine-tune installation behavior by serializing
  // other related command line options, such as "/sessionid <sid>" into
  // `install_settings`.
  base::Value::Dict install_settings_dict;
  install_settings_dict.Set(kEnterpriseSwitch, enterprise);

  std::string install_settings;
  if (!JSONStringValueSerializer(&install_settings)
           .Serialize(install_settings_dict)) {
    VLOG(1) << "Failed to serialize install settings.";
  }

  absl::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
  RegistrationRequest request;
  request.app_id = app_id_;

  absl::optional<tagging::AppArgs> app_args = GetAppArgs(app_id_);
  if (app_args)
    request.ap = app_args->ap;
  if (tag_args)
    request.brand_code = tag_args->brand_code;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&SetUsageStats, GetUpdaterScope(), app_id_,
                     tag_args && tag_args->usage_stats_enable &&
                         *tag_args->usage_stats_enable),
      base::BindOnce(
          &UpdateService::RegisterApp, update_service_, request,
          base::BindOnce(
              [](scoped_refptr<AppInstallControllerImpl> self,
                 const base::FilePath& installer_path,
                 const std::string& install_args,
                 const std::string& install_data,
                 const std::string& install_settings,
                 const RegistrationResponse& response) {
                if (response.status_code != kRegistrationSuccess &&
                    response.status_code != kRegistrationAlreadyRegistered) {
                  VLOG(1) << "Registration failed: " << response.status_code;
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

// TODO(crbug.com/1218219) - propagate error code in case of errors.
void AppInstallControllerImpl::InstallComplete(UpdateService::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == UpdateService::Result::kServiceFailed) {
    UpdateService::UpdateState update_state;
    update_state.app_id = app_id_;
    update_state.state = UpdateService::UpdateState::State::kUpdateError;
    update_state.error_category = UpdateService::ErrorCategory::kService;
    update_state.error_code = -1;
    HandleInstallResult(update_state);
  }

  update_service_ = nullptr;
}

void AppInstallControllerImpl::StateChange(
    const UpdateService::UpdateState& update_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(install_progress_observer_ipc_);

  CHECK_EQ(app_id_, update_state.app_id);

  const auto app_id = base::ASCIIToUTF16(app_id_);
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      install_progress_observer_ipc_->OnCheckingForUpdate();
      break;

    case UpdateService::UpdateState::State::kUpdateAvailable:
      install_progress_observer_ipc_->OnUpdateAvailable(
          app_id, app_name_,
          base::ASCIIToUTF16(update_state.next_version.GetString()));
      break;

    case UpdateService::UpdateState::State::kDownloading: {
      // TODO(sorin): handle time remaining https://crbug.com/1014590.
      const auto pos = GetDownloadProgress(update_state.downloaded_bytes,
                                           update_state.total_bytes);
      install_progress_observer_ipc_->OnDownloading(app_id, app_name_, -1,
                                                    pos != -1 ? pos : 0);
    } break;

    case UpdateService::UpdateState::State::kInstalling: {
      // TODO(sorin): handle the install cancellation.
      // https://crbug.com/1014591
      bool can_start_install = false;
      install_progress_observer_ipc_->OnWaitingToInstall(app_id, app_name_,
                                                         &can_start_install);
      const int pos = update_state.install_progress;
      install_progress_observer_ipc_->OnInstalling(app_id, app_name_, 0,
                                                   pos != -1 ? pos : 0);
      break;
    }

    case UpdateService::UpdateState::State::kUpdated:
    case UpdateService::UpdateState::State::kNoUpdate:
    case UpdateService::UpdateState::State::kUpdateError:
      HandleInstallResult(update_state);
      break;

    case UpdateService::UpdateState::State::kUnknown:
    case UpdateService::UpdateState::State::kNotStarted:
      break;
  }
}

void AppInstallControllerImpl::HandleInstallResult(
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
    default:
      NOTREACHED();
      break;
  }

  ObserverCompletionInfo observer_info;
  observer_info.completion_code = completion_code;
  observer_info.completion_text = completion_text;
  observer_info.help_url =
      base::StringPrintf("%s?product=%s&error=%d", HELP_CENTER_URL,
                         base::EscapeUrlEncodedData(app_id_, false).c_str(),
                         update_state.error_code);

  AppCompletionInfo app_info;
  if (update_state.state != UpdateService::UpdateState::State::kNoUpdate) {
    app_info.app_id = base::ASCIIToUTF16(update_state.app_id);
    app_info.completion_message =
        base::ASCIIToUTF16(update_state.installer_text);
    app_info.error_code = update_state.error_code;
    app_info.extra_code1 = update_state.extra_code1;
    app_info.post_install_launch_command_line =
        base::SysUTF8ToWide(update_state.installer_cmd_line);
    VLOG(1) << app_info.app_id << " installation completed: error_code["
            << app_info.error_code << "], extra_code1[" << app_info.extra_code1
            << "], completion_message[" << app_info.completion_message
            << "], post_install_launch_command_line["
            << app_info.post_install_launch_command_line << "]";

    // TODO(crbug.com/1352307): Figure out how to populate members like
    // `completion_code` and `post_install_url`. For now, set the completion
    // for the basic cases and ignore the post install URL.
    if (app_info.error_code == 0) {
      app_info.completion_code =
          app_info.post_install_launch_command_line.empty()
              ? CompletionCodes::COMPLETION_CODE_SUCCESS
              : CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND;
    } else {
      app_info.completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
    }
  }
  observer_info.apps_info.push_back(app_info);

  install_progress_observer_ipc_->OnComplete(observer_info);
}

// Creates the install progress observer. The observer has thread affinity. It
// must be created, process its messages, and be destroyed on the same thread.
void AppInstallControllerImpl::InitializeUI() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  base::ScopedDisallowBlocking no_blocking_allowed_on_ui_thread;

  ui_message_loop_ = std::make_unique<WTL::CMessageLoop>();
  ui_message_loop_->AddMessageFilter(this);
  ui_thread_id_ = ::GetCurrentThreadId();

  if (is_silent_install_) {
    observer_ = std::make_unique<InstallProgressSilentObserver>(this);
  } else {
    auto progress_wnd =
        std::make_unique<ui::ProgressWnd>(ui_message_loop_.get(), nullptr);
    progress_wnd->SetEventSink(this);
    progress_wnd->Initialize();
    progress_wnd->Show();
    observer_.reset(progress_wnd.release());
  }
}

void AppInstallControllerImpl::RunUI() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());

  ui_message_loop_->Run();
  ui_message_loop_->RemoveMessageFilter(this);

  // This object is owned by the UI thread must be destroyed on this thread.
  observer_ = nullptr;

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback_), kErrorOk));
}

void AppInstallControllerImpl::DoExit() {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
}

BOOL AppInstallControllerImpl::PreTranslateMessage(MSG* msg) {
  if (const auto ui_thread_id = GetUIThreadID(); ui_thread_id != 0) {
    DCHECK_EQ(ui_thread_id, GetCurrentThreadId());
  } else {
    VLOG(1) << "Can't find a thread id for the message: " << msg->message;
  }
  if (msg->message == InstallProgressObserverIPC::WM_PROGRESS_WINDOW_IPC) {
    install_progress_observer_ipc_->Invoke(msg->wParam, msg->lParam);
    return true;
  }
  return false;
}

DWORD AppInstallControllerImpl::GetUIThreadID() const {
  DCHECK_NE(ui_thread_id_, 0u);
  return ui_thread_id_;
}

bool AppInstallControllerImpl::DoLaunchBrowser(const std::string& url) {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  Microsoft::WRL::ComPtr<IShellDispatch2> shell_dispatch;
  base::win::ScopedVariant empty(L"");
#undef ShellExecute
  return GetShellDispatch(&shell_dispatch) &&
         SUCCEEDED(shell_dispatch->ShellExecute(
             base::win::ScopedBstr(base::SysUTF8ToWide(url).c_str()).Get(),
             *empty.AsInput(), *empty.AsInput(), *empty.AsInput(),
             *base::win::ScopedVariant(SW_SHOWNORMAL).AsInput()));
}

bool AppInstallControllerImpl::DoRestartBrowser(
    bool restart_all_browsers,
    const std::vector<std::u16string>& urls) {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  return false;
}

bool AppInstallControllerImpl::DoReboot() {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  return false;
}

void AppInstallControllerImpl::DoCancel() {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateService::CancelInstalls, update_service_, app_id_));
}

}  // namespace

scoped_refptr<App> MakeAppInstall(bool is_silent_install) {
  return base::MakeRefCounted<AppInstall>(
      base::BindRepeating(
          [](const std::string& app_name) -> std::unique_ptr<SplashScreen> {
            return std::make_unique<ui::SplashScreen>(
                base::UTF8ToUTF16(app_name));
          }),
      base::BindRepeating(
          [](bool is_silent_install,
             scoped_refptr<UpdateService> update_service)
              -> scoped_refptr<AppInstallController> {
            return base::MakeRefCounted<AppInstallControllerImpl>(
                is_silent_install, update_service);
          },
          is_silent_install));
}

}  // namespace updater
