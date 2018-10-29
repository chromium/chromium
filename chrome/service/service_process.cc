// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_process_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/service/cloud_print/cloud_print_message_handler.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/net/service_url_request_context_getter.h"
#include "chrome/service/service_process_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/json_pref_store.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_GLIB)
#include <glib-object.h>
#endif

ServiceProcess* g_service_process = NULL;

namespace {

// Delay in seconds after the last service is disabled before we attempt
// a shutdown.
const int kShutdownDelaySeconds = 60;

const char kDefaultServiceProcessLocale[] = "en-US";

class ServiceIOThread : public base::Thread {
 public:
  explicit ServiceIOThread(const char* name);
  ~ServiceIOThread() override;

 protected:
  void CleanUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceIOThread);
};

ServiceIOThread::ServiceIOThread(const char* name) : base::Thread(name) {}
ServiceIOThread::~ServiceIOThread() {
  Stop();
}

void ServiceIOThread::CleanUp() {
  net::URLFetcher::CancelAll();
}

// Prepares the localized strings that are going to be displayed to
// the user if the service process dies. These strings are stored in the
// environment block so they are accessible in the early stages of the
// chrome executable's lifetime.
void PrepareRestartOnCrashEnviroment(
    const base::CommandLine& parsed_command_line) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  // Clear this var so child processes don't show the dialog by default.
  env->UnSetVar(env_vars::kShowRestart);

  // For non-interactive tests we don't restart on crash.
  if (env->HasVar(env_vars::kHeadless))
    return;

  // If the known command-line test options are used we don't create the
  // environment block which means we don't get the restart dialog.
  if (parsed_command_line.HasSwitch(switches::kNoErrorDialogs))
    return;

  // The encoding we use for the info is "title|context|direction" where
  // direction is either env_vars::kRtlLocale or env_vars::kLtrLocale depending
  // on the current locale.
  base::string16 dlg_strings(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_TITLE));
  dlg_strings.push_back('|');
  base::string16 adjusted_string(l10n_util::GetStringFUTF16(
      IDS_SERVICE_CRASH_RECOVERY_CONTENT,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  base::i18n::AdjustStringForLocaleDirection(&adjusted_string);
  dlg_strings.append(adjusted_string);
  dlg_strings.push_back('|');
  dlg_strings.append(base::ASCIIToUTF16(
      base::i18n::IsRTL() ? env_vars::kRtlLocale : env_vars::kLtrLocale));

  env->SetVar(env_vars::kRestartInfo, base::UTF16ToUTF8(dlg_strings));
}

}  // namespace

ServiceProcess::ServiceProcess()
    : shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      enabled_services_(0),
      update_available_(false) {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize(base::OnceClosure quit_closure,
                                const base::CommandLine& command_line,
                                std::unique_ptr<ServiceProcessState> state) {
#if defined(USE_GLIB)
  // g_type_init has been deprecated since version 2.35.
#if !GLIB_CHECK_VERSION(2, 35, 0)
  // Unclear if still needed, but harmless so keeping.
  g_type_init();
#endif
#endif  // defined(USE_GLIB)
  quit_closure_ = std::move(quit_closure);
  service_process_state_ = std::move(state);

  // Initialize TaskScheduler.
  constexpr int kMaxBackgroundThreads = 1;
  constexpr int kMaxBackgroundBlockingThreads = 1;
  constexpr int kMaxForegroundThreads = 3;
  constexpr int kMaxForegroundBlockingThreads = 3;
  constexpr base::TimeDelta kSuggestedReclaimTime =
      base::TimeDelta::FromSeconds(30);

  base::TaskScheduler::Create("CloudPrintServiceProcess");
  base::TaskScheduler::GetInstance()->Start(
      {{kMaxBackgroundThreads, kSuggestedReclaimTime},
       {kMaxBackgroundBlockingThreads, kSuggestedReclaimTime},
       {kMaxForegroundThreads, kSuggestedReclaimTime},
       {kMaxForegroundBlockingThreads, kSuggestedReclaimTime,
        base::SchedulerBackwardCompatibility::INIT_COM_STA}});

  // The NetworkChangeNotifier must be created after TaskScheduler because it
  // posts tasks to it.
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  network_connection_tracker_ =
      std::make_unique<InProcessNetworkConnectionTracker>();

  // Initialize the IO and FILE threads.
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.reset(new ServiceIOThread("ServiceProcess_IO"));
  if (!io_thread_->StartWithOptions(options)) {
    NOTREACHED();
    Teardown();
    return false;
  }

  // Initialize Mojo early so things can use it.
  mojo::core::Init();
  mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
      io_thread_->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));

  request_context_getter_ = new ServiceURLRequestContextGetter();

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath pref_path =
      user_data_dir.Append(chrome::kServiceStateFileName);
  service_prefs_ = std::make_unique<ServiceProcessPrefs>(
      pref_path,
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
          .get());
  service_prefs_->ReadPrefs();

  // This switch it required to run connector with test gaia.
  if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
    net::URLFetcher::SetIgnoreCertificateRequests(true);

  // Check if a locale override has been specified on the command-line.
  std::string locale = command_line.GetSwitchValueASCII(switches::kLang);
  if (!locale.empty()) {
    service_prefs_->SetString(language::prefs::kApplicationLocale, locale);
    service_prefs_->WritePrefs();
  } else {
    // If no command-line value was specified, read the last used locale from
    // the prefs.
    locale = service_prefs_->GetString(language::prefs::kApplicationLocale,
                                       std::string());
    language::ConvertToActualUILocale(&locale);
    // If no locale was specified anywhere, use the default one.
    if (locale.empty())
      locale = kDefaultServiceProcessLocale;
  }
  ui::MaterialDesignController::Initialize();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

  PrepareRestartOnCrashEnviroment(command_line);

  // Enable Cloud Print if needed. First check the command-line.
  // Then check if the cloud print proxy was previously enabled.
  if (command_line.HasSwitch(switches::kEnableCloudPrintProxy) ||
      service_prefs_->GetBoolean(prefs::kCloudPrintProxyEnabled, false)) {
    GetCloudPrintProxy()->EnableForUser();
  }

  VLOG(1) << "Starting Service Process IPC Server";

  ipc_server_.reset(new ServiceIPCServer(this /* client */, io_task_runner(),
                                         &shutdown_event_));
  ipc_server_->binder_registry().AddInterface(
      base::Bind(&cloud_print::CloudPrintMessageHandler::Create, this));
  ipc_server_->Init();

  // After the IPC server has started we signal that the service process is
  // ready.
  if (!service_process_state_->SignalReady(
          io_task_runner().get(),
          base::Bind(&ServiceProcess::Terminate, base::Unretained(this)))) {
    return false;
  }

  // See if we need to stay running.
  ScheduleShutdownCheck();

  return true;
}

bool ServiceProcess::Teardown() {
  service_prefs_.reset();
  cloud_print_proxy_.reset();

  mojo_ipc_support_.reset();
  ipc_server_.reset();

  // On POSIX, this must be called before joining |io_thread_| because it posts
  // a DeleteSoon() task to that thread.
  service_process_state_->SignalStopped();

  // Signal this event before shutting down the service process. That way all
  // background threads can cleanup.
  shutdown_event_.Signal();
  io_thread_.reset();

  if (base::TaskScheduler::GetInstance())
    base::TaskScheduler::GetInstance()->Shutdown();

  // The NetworkChangeNotifier must be destroyed after all other threads that
  // might use it have been shut down.
  network_change_notifier_.reset();

  return true;
}

// This method is called when a shutdown command is received from IPC channel
// or there was an error in the IPC channel.
void ServiceProcess::Shutdown() {
#if defined(OS_MACOSX)
  // On MacOS X the service must be removed from the launchd job list.
  // http://www.chromium.org/developers/design-documents/service-processes
  // The best way to do that is to go through the ForceServiceProcessShutdown
  // path. If it succeeds Terminate() will be called from the handler registered
  // via service_process_state_->SignalReady().
  // On failure call Terminate() directly to force the process to actually
  // terminate.
  if (!ForceServiceProcessShutdown("", 0)) {
    Terminate();
  }
#else
  Terminate();
#endif
}

void ServiceProcess::Terminate() {
  std::move(quit_closure_).Run();
}

void ServiceProcess::OnShutdown() {
  Shutdown();
}

void ServiceProcess::OnUpdateAvailable() {
  update_available_ = true;
}

bool ServiceProcess::OnIPCClientDisconnect() {
  // If there are no enabled services or if there is an update available
  // we want to shutdown right away. Else we want to keep listening for
  // new connections.
  if (!enabled_services_ || update_available_) {
    Shutdown();
    return false;
  }
  return true;
}

mojo::ScopedMessagePipeHandle ServiceProcess::CreateChannelMessagePipe() {
#if defined(OS_MACOSX)
  if (!server_endpoint_.is_valid()) {
    server_endpoint_ =
        service_process_state_->GetServiceProcessServerEndpoint();
    DCHECK(server_endpoint_.is_valid());
  }
#elif defined(OS_POSIX)
  if (!server_endpoint_.is_valid()) {
    mojo::NamedPlatformChannel::Options options;
    options.server_name = service_process_state_->GetServiceProcessServerName();
    mojo::NamedPlatformChannel server_channel(options);
    server_endpoint_ = server_channel.TakeServerEndpoint();
    DCHECK(server_endpoint_.is_valid());
  }
#elif defined(OS_WIN)
  if (server_name_.empty()) {
    server_name_ = service_process_state_->GetServiceProcessServerName();
    DCHECK(!server_name_.empty());
  }
#endif

  mojo::PlatformChannelServerEndpoint server_endpoint;
#if defined(OS_POSIX)
  server_endpoint = server_endpoint_.Clone();
#elif defined(OS_WIN)
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name_;
  options.enforce_uniqueness = false;
  mojo::NamedPlatformChannel server_channel(options);
  server_endpoint = server_channel.TakeServerEndpoint();
#endif
  CHECK(server_endpoint.is_valid());

  mojo_connection_ = std::make_unique<mojo::IsolatedConnection>();
  return mojo_connection_->Connect(std::move(server_endpoint));
}

cloud_print::CloudPrintProxy* ServiceProcess::GetCloudPrintProxy() {
  if (!cloud_print_proxy_.get()) {
    cloud_print_proxy_.reset(new cloud_print::CloudPrintProxy());
    cloud_print_proxy_->Initialize(service_prefs_.get(), this,
                                   network_connection_tracker_.get());
  }
  return cloud_print_proxy_.get();
}

void ServiceProcess::OnCloudPrintProxyEnabled(bool persist_state) {
  if (persist_state) {
    // Save the preference that we have enabled the cloud print proxy.
    service_prefs_->SetBoolean(prefs::kCloudPrintProxyEnabled, true);
    service_prefs_->WritePrefs();
  }
  OnServiceEnabled();
}

void ServiceProcess::OnCloudPrintProxyDisabled(bool persist_state) {
  if (persist_state) {
    // Save the preference that we have disabled the cloud print proxy.
    service_prefs_->SetBoolean(prefs::kCloudPrintProxyEnabled, false);
    service_prefs_->WritePrefs();
  }
  OnServiceDisabled();
}

ServiceURLRequestContextGetter*
ServiceProcess::GetServiceURLRequestContextGetter() {
  DCHECK(request_context_getter_.get());
  return request_context_getter_.get();
}

void ServiceProcess::OnServiceEnabled() {
  enabled_services_++;
  if ((1 == enabled_services_) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoServiceAutorun)) {
    if (!service_process_state_->AddToAutoRun()) {
      // TODO(scottbyer/sanjeevr/dmaclach): Handle error condition
      LOG(ERROR) << "Unable to AddToAutoRun";
    }
  }
}

void ServiceProcess::OnServiceDisabled() {
  DCHECK_NE(enabled_services_, 0);
  enabled_services_--;
  if (0 == enabled_services_) {
    if (!service_process_state_->RemoveFromAutoRun()) {
      // TODO(scottbyer/sanjeevr/dmaclach): Handle error condition
      LOG(ERROR) << "Unable to RemoveFromAutoRun";
    }
    // We will wait for some time to respond to IPCs before shutting down.
    ScheduleShutdownCheck();
  }
}

void ServiceProcess::ScheduleShutdownCheck() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServiceProcess::ShutdownIfNeeded, base::Unretained(this)),
      base::TimeDelta::FromSeconds(kShutdownDelaySeconds));
}

void ServiceProcess::ShutdownIfNeeded() {
  if (0 == enabled_services_) {
    if (ipc_server_->is_ipc_client_connected()) {
      // If there is an IPC client connected, we need to try again later.
      // Note that there is still a timing window here because a client may
      // decide to connect at this point.
      // TODO(sanjeevr): Fix this timing window.
      ScheduleShutdownCheck();
    } else {
      Shutdown();
    }
  }
}

ServiceProcess::~ServiceProcess() {
  Teardown();
  g_service_process = NULL;
}
