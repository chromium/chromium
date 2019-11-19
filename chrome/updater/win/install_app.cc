// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/install_app.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/atl.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/win/install_progress_observer.h"
#include "chrome/updater/win/setup/setup.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/splash_screen.h"
#include "chrome/updater/win/ui/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace updater {

namespace {

// TODO(sorin): remove the hardcoding of the application name.
// https://crbug.com/1014298
constexpr base::char16 kAppNameChrome[] = L"Google Chrome";

class InstallAppController;

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

  explicit InstallProgressObserverIPC(ui::ProgressWnd* progress_wnd);
  ~InstallProgressObserverIPC() override;

  // Called by the window proc when a specific application message is processed
  // by the progress window. This call always occurs in the context of the
  // thread which owns the window.
  void Invoke(WPARAM wparam, LPARAM lparam);

  // Overrides for InstallProgressObserver.
  // Called by the application installer code on the main udpater thread.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const base::string16& app_id,
                         const base::string16& app_name,
                         const base::string16& version_string) override;
  void OnWaitingToDownload(const base::string16& app_id,
                           const base::string16& app_name) override;
  void OnDownloading(const base::string16& app_id,
                     const base::string16& app_name,
                     int time_remaining_ms,
                     int pos) override;
  void OnWaitingRetryDownload(const base::string16& app_id,
                              const base::string16& app_name,
                              const base::Time& next_retry_time) override;
  void OnWaitingToInstall(const base::string16& app_id,
                          const base::string16& app_name,
                          bool* can_start_install) override;
  void OnInstalling(const base::string16& app_id,
                    const base::string16& app_name,
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
    ParamOnUpdateAvailable();
    base::string16 app_id;
    base::string16 app_name;
    base::string16 version_string;

   private:
    DISALLOW_COPY_AND_ASSIGN(ParamOnUpdateAvailable);
  };

  struct ParamOnDownloading {
    ParamOnDownloading();
    base::string16 app_id;
    base::string16 app_name;
    int time_remaining_ms = 0;
    int pos = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ParamOnDownloading);
  };

  struct ParamOnComplete {
    ParamOnComplete();
    ObserverCompletionInfo observer_info;

   private:
    DISALLOW_COPY_AND_ASSIGN(ParamOnComplete);
  };

  THREAD_CHECKER(thread_checker_);

  // This member is not owned by this class.
  ui::ProgressWnd* progress_wnd_ = nullptr;

  // The thread id of the thread which owns the |ProgressWnd|.
  int window_thread_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InstallProgressObserverIPC);
};

InstallProgressObserverIPC::ParamOnUpdateAvailable::ParamOnUpdateAvailable() =
    default;
InstallProgressObserverIPC::ParamOnDownloading::ParamOnDownloading() = default;
InstallProgressObserverIPC::ParamOnComplete::ParamOnComplete() = default;

InstallProgressObserverIPC::InstallProgressObserverIPC(
    ui::ProgressWnd* progress_wnd)
    : progress_wnd_(progress_wnd),
      window_thread_id_(
          ::GetWindowThreadProcessId(progress_wnd_->m_hWnd, nullptr)) {
  DCHECK(progress_wnd);
  DCHECK(progress_wnd->m_hWnd);
  DCHECK(IsWindow(progress_wnd->m_hWnd));
}
InstallProgressObserverIPC::~InstallProgressObserverIPC() = default;

void InstallProgressObserverIPC::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnCheckingForUpdate),
                      0);
}

void InstallProgressObserverIPC::OnUpdateAvailable(
    const base::string16& app_id,
    const base::string16& app_name,
    const base::string16& version_string) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnUpdateAvailable> param_on_update_available =
      std::make_unique<ParamOnUpdateAvailable>();
  param_on_update_available->app_id = app_id;
  param_on_update_available->app_name = app_name;
  param_on_update_available->version_string = version_string;
  ::PostThreadMessage(
      window_thread_id_, WM_PROGRESS_WINDOW_IPC,
      static_cast<WPARAM>(IPCAppMessages::kOnUpdateAvailable),
      reinterpret_cast<LPARAM>(param_on_update_available.release()));
}

void InstallProgressObserverIPC::OnWaitingToDownload(
    const base::string16& app_id,
    const base::string16& app_name) {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnDownloading(const base::string16& app_id,
                                               const base::string16& app_name,
                                               int time_remaining_ms,
                                               int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnDownloading> param_on_downloading =
      std::make_unique<ParamOnDownloading>();
  param_on_downloading->app_id = app_id;
  param_on_downloading->app_name = app_name;
  param_on_downloading->time_remaining_ms = time_remaining_ms;
  param_on_downloading->pos = pos;
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnDownloading),
                      reinterpret_cast<LPARAM>(param_on_downloading.release()));
}

void InstallProgressObserverIPC::OnWaitingRetryDownload(
    const base::string16& app_id,
    const base::string16& app_name,
    const base::Time& next_retry_time) {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnWaitingToInstall(
    const base::string16& app_id,
    const base::string16& app_name,
    bool* can_start_install) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void InstallProgressObserverIPC::OnInstalling(const base::string16& app_id,
                                              const base::string16& app_name,
                                              int time_remaining_ms,
                                              int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(sorin): implement progress, https://crbug.com/1014594.
}

void InstallProgressObserverIPC::OnPause() {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnComplete> param_on_complete =
      std::make_unique<ParamOnComplete>();
  param_on_complete->observer_info = observer_info;
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnComplete),
                      reinterpret_cast<LPARAM>(param_on_complete.release()));
}

void InstallProgressObserverIPC::Invoke(WPARAM wparam, LPARAM lparam) {
  DCHECK_EQ(::GetWindowThreadProcessId(progress_wnd_->m_hWnd, nullptr),
            ::GetCurrentThreadId());
  auto* observer = static_cast<InstallProgressObserver*>(progress_wnd_);
  switch (static_cast<IPCAppMessages>(wparam)) {
    case IPCAppMessages::kOnCheckingForUpdate:
      observer->OnCheckingForUpdate();
      break;
    case IPCAppMessages::kOnUpdateAvailable: {
      std::unique_ptr<ParamOnUpdateAvailable> param_on_update_available(
          reinterpret_cast<ParamOnUpdateAvailable*>(lparam));
      observer->OnUpdateAvailable(param_on_update_available->app_id,
                                  param_on_update_available->app_name,
                                  param_on_update_available->version_string);
      break;
    }
    case IPCAppMessages::kOnDownloading: {
      std::unique_ptr<ParamOnDownloading> param_on_downloading(
          reinterpret_cast<ParamOnDownloading*>(lparam));
      observer->OnDownloading(
          param_on_downloading->app_id, param_on_downloading->app_name,
          param_on_downloading->time_remaining_ms, param_on_downloading->pos);
      break;
    }
    case IPCAppMessages::kOnComplete: {
      std::unique_ptr<ParamOnComplete> param_on_complete(
          reinterpret_cast<ParamOnComplete*>(lparam));
      observer->OnComplete(param_on_complete->observer_info);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// Implements installing a single application by invoking the code in
// |update_client|, listening to |update_client| and UI events, and
// driving the UI code by calling the functions exposed by
// |InstallProgressObserver|. This class is an observer for an install which is
// handled by |update_client|, and it also notifies the UI, which is an
// observer of this class.
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
class InstallAppController : public ui::ProgressWndEvents,
                             public update_client::UpdateClient::Observer,
                             public WTL::CMessageFilter {
 public:
  InstallAppController();
  ~InstallAppController() override;

  int InstallApp(const std::string& app_id);

 private:
  // Overrides for update_client::UpdateClient::Observer. This function is
  // called on the main updater thread.
  void OnEvent(Events event, const std::string& id) override;

  // Overrides for OmahaWndEvents. These functions are called on the UI thread.
  void DoClose() override {}
  void DoExit() override;

  // Overrides for CompleteWndEvents. This function is called on the UI thread.
  bool DoLaunchBrowser(const base::string16& url) override { return false; }

  // Overrides for ProgressWndEvents. These functions are called on the UI
  // thread.
  bool DoRestartBrowser(bool restart_all_browsers,
                        const std::vector<base::string16>& urls) override {
    return false;
  }
  bool DoReboot() override { return false; }
  void DoCancel() override {}

  // Overrides for WTL::CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  // These functions are called on the UI thread.
  void InitializeUI();
  void RunUI();

  // These functions are called on the main updater thread.
  void DoInstallApp(update_client::CrxComponent component);
  void InstallComplete();
  void HandleInstallResult(Events event,
                           const update_client::CrxUpdateItem& update_item);
  void QuitRunLoop();
  void FlushPrefs();

  // Returns the thread id of the thread which owns the progress window.
  DWORD GetUIThreadID() const;

  THREAD_CHECKER(thread_checker_);

  // Provides an execution environment for the updater main thread.
  base::SingleThreadTaskExecutor main_task_executor_;

  // Provides an execution environment for the UI code. Typically, it runs
  // a single task which is the UI run loop.
  scoped_refptr<base::TaskRunner> ui_task_runner_;

  // The run loop associated with the updater main thread.
  base::RunLoop runloop_;

  // The application ID and the associated application name. The application
  // name is displayed by the UI and it must be i18n.
  std::string app_id_;
  const base::string16 app_name_;

  // The |update_client| objects and dependencies.
  scoped_refptr<update_client::Configurator> config_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  std::unique_ptr<Observer> observer_;

  // The message loop associated with the UI.
  std::unique_ptr<WTL::CMessageLoop> ui_message_loop_;

  // The progress window.
  std::unique_ptr<ui::ProgressWnd> progress_wnd_;

  // The adapter for the inter-thread calls between the updater main thread
  // and the UI thread.
  std::unique_ptr<InstallProgressObserverIPC> install_progress_observer_ipc_;

  DISALLOW_COPY_AND_ASSIGN(InstallAppController);
};

// TODO(sorin): fix the hardcoding of the application name.
// https:crbug.com/1014298
InstallAppController::InstallAppController()
    : main_task_executor_(base::MessagePumpType::UI),
      ui_task_runner_(base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      app_name_(kAppNameChrome),
      config_(base::MakeRefCounted<Configurator>()) {}

InstallAppController::~InstallAppController() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

int InstallAppController::InstallApp(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  base::i18n::InitializeICU();
  base::ScopedDisallowBlocking no_blocking_allowed_on_main_thread;

  app_id_ = app_id;

  // Creates a CRX installer bound to this |app_id| on a blocking task runner,
  // since such code requires accessing the file system. Once this task
  // completes, the reply initializes the UI code on the UI thread, and then,
  // it invokes |DoInstallApp| on the main updater thread.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](const std::string& app_id) {
            auto installer = base::MakeRefCounted<Installer>(app_id);
            installer->FindInstallOfApp();
            return installer->MakeCrxComponent();
          },
          app_id_),
      base::BindOnce(
          [](InstallAppController* controller,
             update_client::CrxComponent component) {
            controller->ui_task_runner_->PostTaskAndReply(
                FROM_HERE,
                base::BindOnce(&InstallAppController::InitializeUI,
                               base::Unretained(controller)),
                base::BindOnce(&InstallAppController::DoInstallApp,
                               base::Unretained(controller),
                               std::move(component)));
          },
          this));
  runloop_.Run();
  FlushPrefs();
  return 0;
}

void InstallAppController::DoInstallApp(update_client::CrxComponent component) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // At this point, the UI has been initialized, which means the UI can be
  // used from now on as an observer of the application install. The task
  // below runs the UI message loop for the UI until it exits, because
  // a WM_QUIT message has been posted to it.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InstallAppController::RunUI, base::Unretained(this)));

  update_client_ = update_client::UpdateClientFactory(config_);
  update_client_->AddObserver(this);

  install_progress_observer_ipc_ =
      std::make_unique<InstallProgressObserverIPC>(progress_wnd_.get());

  update_client_->Install(
      app_id_,
      base::BindOnce(
          [](const update_client::CrxComponent& component,
             const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            DCHECK_EQ(1u, ids.size());
            return {component};
          },
          std::move(component)),
      base::BindOnce(
          [](InstallAppController* install_app_controller,
             update_client::Error error) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(&InstallAppController::InstallComplete,
                               base::Unretained(install_app_controller)));
          },
          base::Unretained(this)));
}

// This function is invoked after the |update_client::Install| call has been
// fully handled and the state associated with the application ID has been
// destroyed. The caller of |update_client::Install| must listen to
// Observer::OnEvent to get completion status instead of querying the status
// by calling UpdateClient::GetCrxUpdateState.
void InstallAppController::InstallComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  install_progress_observer_ipc_ = nullptr;
  update_client_->RemoveObserver(this);
  update_client_ = nullptr;
}

void InstallAppController::OnEvent(Events event, const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(install_progress_observer_ipc_);

  CHECK_EQ(app_id_, id);

  update_client::CrxUpdateItem crx_update_item;
  update_client_->GetCrxUpdateState(app_id_, &crx_update_item);

  const auto app_id = base::ASCIIToUTF16(app_id_);
  switch (event) {
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
      install_progress_observer_ipc_->OnCheckingForUpdate();
      break;

    case Events::COMPONENT_UPDATE_FOUND:
      install_progress_observer_ipc_->OnUpdateAvailable(
          app_id, app_name_,
          base::ASCIIToUTF16(crx_update_item.next_version.GetString()));
      break;

    case Events::COMPONENT_UPDATE_DOWNLOADING:
      // TODO(sorin): handle progress and time remaining.
      // https://crbug.com/1014590
      install_progress_observer_ipc_->OnDownloading(app_id, app_name_, -1, 0);
      break;

    case Events::COMPONENT_UPDATE_READY: {
      // TODO(sorin): handle the install cancellation.
      // https://crbug.com/1014591
      bool can_start_install = false;
      install_progress_observer_ipc_->OnWaitingToInstall(app_id, app_name_,
                                                         &can_start_install);

      // TODO(sorin): handle progress and time remaining.
      // https://crbug.com/1014594
      install_progress_observer_ipc_->OnInstalling(app_id, app_name_, 0, 0);
      break;
    }

    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_NOT_UPDATED:
    case Events::COMPONENT_UPDATE_ERROR:
      HandleInstallResult(event, crx_update_item);
      break;

    case Events::COMPONENT_WAIT:
    default:
      NOTREACHED();
      break;
  }
}

void InstallAppController::HandleInstallResult(
    Events event,
    const update_client::CrxUpdateItem& update_item) {
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
  base::string16 completion_text;
  switch (update_item.state) {
    case update_client::ComponentState::kUpdated:
      VLOG(1) << "Update success.";
      completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
      ui::LoadString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY, &completion_text);
      break;
    case update_client::ComponentState::kUpToDate:
      VLOG(1) << "No updates.";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      ui::LoadString(IDS_NO_UPDATE_RESPONSE, &completion_text);
      break;
    case update_client::ComponentState::kUpdateError:
      VLOG(1) << "Updater error: " << update_item.error_code << ".";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      ui::LoadString(IDS_INSTALL_FAILED, &completion_text);
      break;
    default:
      NOTREACHED();
      break;
  }

  ObserverCompletionInfo observer_info;
  observer_info.completion_code = completion_code;
  observer_info.completion_text = completion_text;
  // TODO(sorin): implement handling the help URL. https://crbug.com/1014622
  observer_info.help_url = L"http://www.google.com";
  // TODO(sorin): implement the installer API and provide the
  // application info in the observer info. https://crbug.com/1014630
  observer_info.apps_info.push_back({});
  install_progress_observer_ipc_->OnComplete(observer_info);
}

// Creates and shows the progress window. The window has thread affinity. It
// must be created, process its messages, and be destroyed on the same thread.
void InstallAppController::InitializeUI() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  base::ScopedDisallowBlocking no_blocking_allowed_on_ui_thread;

  ui_message_loop_ = std::make_unique<WTL::CMessageLoop>();
  ui_message_loop_->AddMessageFilter(this);
  progress_wnd_ =
      std::make_unique<ui::ProgressWnd>(ui_message_loop_.get(), nullptr);
  progress_wnd_->SetEventSink(this);
  progress_wnd_->Initialize();
  progress_wnd_->Show();
}

void InstallAppController::RunUI() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());

  ui_message_loop_->Run();
  ui_message_loop_->RemoveMessageFilter(this);

  // This object is owned by the UI thread must be destroyed on this thread.
  progress_wnd_ = nullptr;

  main_task_executor_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InstallAppController::QuitRunLoop,
                                base::Unretained(this)));
}

void InstallAppController::DoExit() {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
}

BOOL InstallAppController::PreTranslateMessage(MSG* msg) {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  if (msg->message == InstallProgressObserverIPC::WM_PROGRESS_WINDOW_IPC) {
    install_progress_observer_ipc_->Invoke(msg->wParam, msg->lParam);
    return true;
  }
  return false;
}

void InstallAppController::FlushPrefs() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::RunLoop runloop;
  config_->GetPrefService()->CommitPendingWrite(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      runloop.QuitWhenIdleClosure()));
  runloop.Run();
}

void InstallAppController::QuitRunLoop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  runloop_.QuitWhenIdleClosure().Run();
}

DWORD InstallAppController::GetUIThreadID() const {
  DCHECK(progress_wnd_);
  return ::GetWindowThreadProcessId(progress_wnd_->m_hWnd, nullptr);
}

}  // namespace

int SetupUpdater() {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop runloop;
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  base::ScopedDisallowBlocking no_blocking_allowed;

  ui::SplashScreen splash_screen(kAppNameChrome);
  splash_screen.Show();

  int setup_result = 0;
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce([]() { return Setup(); }),
      base::BindOnce(
          [](ui::SplashScreen* splash_screen, base::OnceClosure quit_closure,
             int* result_out, int result) {
            *result_out = result;
            splash_screen->Dismiss(base::BindOnce(
                [](base::OnceClosure quit_closure) {
                  std::move(quit_closure).Run();
                },
                std::move(quit_closure)));
          },
          &splash_screen, runloop.QuitWhenIdleClosure(), &setup_result));

  runloop.Run();

  return setup_result;
}

int InstallApp(const std::string& app_id) {
  int result = SetupUpdater();
  if (result)
    return result;

  return InstallAppController().InstallApp(app_id);
}

}  // namespace updater
