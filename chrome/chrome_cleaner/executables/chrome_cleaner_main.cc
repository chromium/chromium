// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <commctrl.h>
#include <psapi.h>

#include <memory>
#include <set>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/components/recovery_component.h"
#include "chrome/chrome_cleaner/components/system_report_component.h"
#include "chrome/chrome_cleaner/components/system_restore_point_component.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/chrome_cleanup_tool_branding.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/crash/crash_client.h"
#include "chrome/chrome_cleaner/crash/crash_reporter.h"
#include "chrome/chrome_cleaner/engines/broker/interface_log_service.h"
#include "chrome/chrome_cleaner/engines/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/engines/controllers/elevating_facade.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade_interface.h"
#include "chrome/chrome_cleaner/engines/controllers/main_controller.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate_factory.h"
#include "chrome/chrome_cleaner/engines/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/executables/shutdown_sequence.h"
#include "chrome/chrome_cleaner/ipc/mojo_chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/proto_chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/pending_logs_service.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "chrome/chrome_cleaner/os/post_reboot_registration.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/rebooter.h"
#include "chrome/chrome_cleaner/os/secure_dll_loading.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/parsers/json_parser/sandboxed_json_parser.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"
#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner_impl.h"
#include "chrome/chrome_cleaner/settings/engine_settings.h"
#include "chrome/chrome_cleaner/settings/matching_options.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace {

using chrome_cleaner::ExecutionMode;

// The number of milliseconds to sleep to delay the self-deletion.
const uint32_t kSelfDeleteDelayMs = 1000;

const wchar_t kElevatedLogFileSuffix[] = L"-elevated";

// A callback for the logs service to call us back when it's done with logs
// upload. |success| is the result of the upload, and |succeeded|, when not
// null, is set with the |success| value.
void LogsUploadCallback(bool* succeeded,
                        base::OnceClosure quit_closure,
                        bool success) {
  if (succeeded)
    *succeeded = success;
  // Use a task instead of a direct call to QuitWhenIdle, in case we are called
  // synchronously because of an upload error, and the task executor is not
  // running yet.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                std::move(quit_closure));
}

void AddComponents(chrome_cleaner::MainController* main_controller,
                   base::CommandLine* command_line,
                   chrome_cleaner::JsonParserAPI* json_parser,
                   chrome_cleaner::SandboxedShortcutParser* shortcut_parser) {
#if BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  // Ensure that the system restore point component runs first.
  main_controller->AddComponent(
      std::make_unique<chrome_cleaner::SystemRestorePointComponent>(
          PRODUCT_FULLNAME_STRING));
#endif

  if (chrome_cleaner::RecoveryComponent::IsAvailable())
    main_controller->AddComponent(
        std::make_unique<chrome_cleaner::RecoveryComponent>());

  main_controller->AddComponent(
      std::make_unique<chrome_cleaner::SystemReportComponent>(json_parser,
                                                              shortcut_parser));
}

void SendLogsToSafeBrowsing(chrome_cleaner::ResultCode exit_code,
                            chrome_cleaner::RegistryLogger* registry_logger) {
  chrome_cleaner::LoggingServiceAPI* logging_service =
      chrome_cleaner::LoggingServiceAPI::GetInstance();
  logging_service->SetExitCode(exit_code);
  base::RunLoop run_loop;
  logging_service->SendLogsToSafeBrowsing(
      base::BindRepeating(&LogsUploadCallback, nullptr,
                          run_loop.QuitWhenIdleClosure()),
      registry_logger);
  run_loop.Run();
}

chrome_cleaner::ResultCode RelaunchElevated(
    chrome_cleaner::RegistryLogger* registry_logger) {
  // If this is being done after we tried to relaunch elevated, there is a
  // problem. We unfortunately can't report it since the user didn't get a
  // chance to opt out of logs upload.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chrome_cleaner::kElevatedSwitch)) {
    LOG(ERROR) << "Failed to restart elevated.";
    return chrome_cleaner::RESULT_CODE_FAILED_TO_ELEVATE;
  }

  command_line->AppendSwitch(chrome_cleaner::kElevatedSwitch);
  base::Process elevated_process =
      chrome_cleaner::LaunchElevatedProcessWithAssociatedWindow(
          *command_line,
          /*hwnd=*/nullptr);
  if (elevated_process.IsValid()) {
    LOG(INFO) << "Successfully re-launched elevated.";
  } else {
    LOG(ERROR) << "Failed to re-launch elevated.";
    chrome_cleaner::Settings* settings =
        chrome_cleaner::Settings::GetInstance();
    if (chrome_cleaner::Rebooter::IsPostReboot()) {
      chrome_cleaner::ResultCode exit_code =
          chrome_cleaner::RESULT_CODE_POST_REBOOT_ELEVATION_DENIED;
      if (settings->logs_upload_allowed())
        SendLogsToSafeBrowsing(exit_code, registry_logger);
      return exit_code;
    }
    // In legacy mode, we can't upload logs to Safe Browsing if this is not
    // a post-reboot run. We log a distinct exit code to indicate in UMA that
    // elevation was declined or the user doesn't have admin rights.
    return chrome_cleaner::RESULT_CODE_ELEVATION_PROMPT_DECLINED;
  }
  return chrome_cleaner::RESULT_CODE_SUCCESS;
}

// Run the chrome Cleaner to scan and clean.
chrome_cleaner::ResultCode RunChromeCleaner(
    base::CommandLine* command_line,
    chrome_cleaner::RebooterAPI* rebooter,
    chrome_cleaner::RegistryLogger* registry_logger,
    chrome_cleaner::ChromePromptIPC* chrome_prompt_ipc,
    chrome_cleaner::ShutdownSequence shutdown_sequence) {
  if (command_line->HasSwitch(chrome_cleaner::kCrashSwitch)) {
    int* crash_me = nullptr;
    *crash_me = 42;
  }

  INITCOMMONCONTROLSEX common_control_info = {sizeof(INITCOMMONCONTROLSEX),
                                              ICC_LINK_CLASS};
  if (!InitCommonControlsEx(&common_control_info))
    return chrome_cleaner::RESULT_CODE_FAILED;

  // There is a circular dependency: MainController depends on EngineFacade;
  // EngineFacade might instantiate a sandbox but the sandbox connection error
  // handler invokes a method of MainController.
  //
  // To break this circle, create MainController first and get the error
  // handler with MainController::GetSandboxConnectionErrorCallback. Then
  // create the EngineFacade with the error handler, and set it on the
  // MainController. It's important that the connection error handler is
  // available when the EngineFacade is created, otherwise there's a race
  // condition where a sandbox target process can be spawned without an error
  // handler, so any disconnection before MainController is created wouldn't be
  // handled.

  chrome_cleaner::MainController main_controller(rebooter, registry_logger,
                                                 chrome_prompt_ipc);
  chrome_cleaner::SandboxConnectionErrorCallback connection_error_callback =
      main_controller.GetSandboxConnectionErrorCallback();

  // Initialize a null RemoteParserPtr to be set by SpawnParserSandbox.
  chrome_cleaner::RemoteParserPtr parser(nullptr,
                                         base::OnTaskRunnerDeleter(nullptr));
  chrome_cleaner::ResultCode init_result = chrome_cleaner::SpawnParserSandbox(
      shutdown_sequence.mojo_task_runner, connection_error_callback, &parser);
  if (init_result != chrome_cleaner::RESULT_CODE_SUCCESS) {
    return init_result;
  }
  std::unique_ptr<chrome_cleaner::SandboxedJsonParser> json_parser =
      std::make_unique<chrome_cleaner::SandboxedJsonParser>(
          shutdown_sequence.mojo_task_runner.get(), parser.get());
  std::unique_ptr<chrome_cleaner::SandboxedShortcutParser> shortcut_parser =
      std::make_unique<chrome_cleaner::SandboxedShortcutParser>(
          shutdown_sequence.mojo_task_runner.get(), parser.get());

  chrome_cleaner::Settings* settings = chrome_cleaner::Settings::GetInstance();
  if (!chrome_cleaner::IsSupportedEngine(settings->engine())) {
    LOG(FATAL) << "Unsupported engine " << settings->engine();
    return chrome_cleaner::RESULT_CODE_FAILED;
  }

  chrome_cleaner::InitializePUPDataWithCatalog(settings->engine());

  std::unique_ptr<chrome_cleaner::InterfaceLogService> interface_log_service =
      chrome_cleaner::InterfaceLogService::Create(
          command_line->GetSwitchValueNative(
              chrome_cleaner::kLogInterfaceCallsToSwitch),
          CHROME_CLEANER_VERSION_STRING);

  chrome_cleaner::ResultCode engine_result;
  std::tie(engine_result, shutdown_sequence.engine_client) = SpawnEngineSandbox(
      settings->engine(), registry_logger, shutdown_sequence.mojo_task_runner,
      connection_error_callback, std::move(interface_log_service));
  if (engine_result != chrome_cleaner::RESULT_CODE_SUCCESS)
    return engine_result;

  shutdown_sequence
      .engine_facade = std::make_unique<chrome_cleaner::EngineFacade>(
      shutdown_sequence.engine_client, json_parser.get(),
      main_controller.main_dialog(),
      std::make_unique<chrome_cleaner::ForceInstalledExtensionScannerImpl>(),
      chrome_prompt_ipc);

  if (settings->execution_mode() == ExecutionMode::kScanning) {
    shutdown_sequence.engine_facade =
        std::make_unique<chrome_cleaner::ElevatingFacade>(
            std::move(shutdown_sequence.engine_facade));
  }
  main_controller.SetEngineFacade(shutdown_sequence.engine_facade.get());

  // Ensure profile reset, recovery and other components run only once by
  // running them only in the cleaning mode.
  if (settings->execution_mode() != ExecutionMode::kScanning) {
    AddComponents(&main_controller, command_line, json_parser.get(),
                  shortcut_parser.get());
  }

  command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chrome_cleaner::kLoadEmptyDLLSwitch)) {
    chrome_cleaner::testing::LoadEmptyDLL();
  }
  chrome_cleaner::NotifyInitializationDoneForTesting();

  if (chrome_cleaner::Rebooter::IsPostReboot()) {
    // When running post reboot, confirm whether the job was done successfully
    // or not.
    return main_controller.ValidateCleanup();
  }

  return main_controller.ScanAndClean();
}

// Return false when a self delete should NOT not be attempted.
bool CanSelfDelete(chrome_cleaner::ResultCode exit_code) {
  if (exit_code == chrome_cleaner::RESULT_CODE_PENDING_REBOOT ||
      exit_code == chrome_cleaner::RESULT_CODE_CANCELED ||
      exit_code == chrome_cleaner::RESULT_CODE_CLEANUP_PROMPT_DENIED) {
    return false;
  }
  std::unique_ptr<chrome_cleaner::TaskScheduler> task_scheduler(
      chrome_cleaner::TaskScheduler::CreateInstance());
  return !task_scheduler->IsTaskRegistered(
      chrome_cleaner::PendingLogsService::LogsUploadRetryTaskName(
          PRODUCT_SHORTNAME_STRING)
          .c_str());
}

chrome_cleaner::ResultCode ReturnWithResultCode(
    chrome_cleaner::ResultCode result_code,
    const base::FilePath& exe_path,
    chrome_cleaner::RegistryLogger* registry_logger,
    chrome_cleaner::RebooterAPI* rebooter) {
  DCHECK_NE(chrome_cleaner::RESULT_CODE_INVALID, result_code);

  registry_logger->WriteExitCode(result_code);
  registry_logger->WriteEndTime();

  bool self_delete = CanSelfDelete(result_code);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  self_delete = self_delete &&
                !command_line->HasSwitch(chrome_cleaner::kNoSelfDeleteSwitch);
#else
  self_delete = self_delete &&
                command_line->HasSwitch(chrome_cleaner::kForceSelfDeleteSwitch);
#endif

  if (self_delete) {
    LOG(INFO) << "Self-deleting.";

    if (!chrome_cleaner::DeleteFileFromTempProcess(exe_path, kSelfDeleteDelayMs,
                                                   nullptr)) {
      PLOG(ERROR) << "Failed to self-DeleteFileFromTempProcess.";
    }

    // Embedded libraries may have been extracted. Try to delete them and ignore
    // errors.
    base::FilePath exe_dir = exe_path.DirName();
    std::set<base::string16> embedded_libraries =
        chrome_cleaner::GetLibrariesToLoad(
            chrome_cleaner::Settings::GetInstance()->engine());
    for (const auto& library : embedded_libraries) {
      chrome_cleaner::DeleteFileFromTempProcess(exe_dir.Append(library),
                                                kSelfDeleteDelayMs, nullptr);
    }
  }

  if (result_code == chrome_cleaner::RESULT_CODE_SUCCESS ||
      result_code == chrome_cleaner::RESULT_CODE_POST_REBOOT_SUCCESS) {
    registry_logger->RecordCompletedCleanup();
  }

  LOG(INFO) << "Exiting with code: " << result_code;
  chrome_cleaner::TaskScheduler::Terminate();

  return result_code;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
  // This must be executed as soon as possible to reduce the number of dlls that
  // the code might try to load before we can lock things down.
  chrome_cleaner::EnableSecureDllLoading();

  base::AtExitManager at_exit;

  // This must be done BEFORE constructing ScopedLogging, which call InitLogging
  // to set the name of the log file, which needs to read from the command line.
  bool success = base::CommandLine::Init(0, nullptr);
  DCHECK(success);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // The list of post-reboot switches has grown so long that RunOnce no longer
  // works, as there is a limit of 260 characters on RunOnce values. Switches
  // are stored in a separate registry key.
  if (command_line->HasSwitch(
          chrome_cleaner::kPostRebootSwitchesInOtherRegistryKeySwitch)) {
    base::CommandLine tmp_cmd(command_line->GetProgram());
    chrome_cleaner::PostRebootRegistration post_reboot(
        PRODUCT_SHORTNAME_STRING);
    if (!post_reboot.ReadRunOncePostRebootCommandLine(
            command_line->GetSwitchValueASCII(chrome_cleaner::kCleanupIdSwitch),
            &tmp_cmd)) {
      LOG(DFATAL)
          << "Could not read post-reboot switches from the registry key";
      // This shouldn't be an attack vector by UwS: the TaskScheduler should
      // run the cleaner if the RunOnce attempt failed.
      return chrome_cleaner::RESULT_CODE_FAILED;
    }

    // Overwrites the current process' command line. Future calls to
    // base::CommandLine::ForCurrentProcess will return the mutated |tmp_cmd|
    // instead of the original command line.
    // Note that this mutation is not thread safe.
    *command_line = tmp_cmd;
  }

  chrome_cleaner::ShutdownSequence shutdown_sequence;

  chrome_cleaner::RegistryLogger registry_logger(
      chrome_cleaner::RegistryLogger::Mode::REMOVER);
  if (!chrome_cleaner::InitializeOSUtils()) {
    return ReturnWithResultCode(
        chrome_cleaner::RESULT_CODE_INITIALIZATION_ERROR, base::FilePath(),
        &registry_logger, nullptr);
  }

  const char* crash_reporter_switch = chrome_cleaner::kCrashHandlerSwitch;
  if (command_line->HasSwitch(crash_reporter_switch) &&
      !command_line->HasSwitch(chrome_cleaner::kUploadLogFileSwitch)) {
    // If this process should run as the crash reporter, run that then return
    // immediately, as this process is not meant to be the cleaner itself.
    return CrashReporterMain();
  }

  // GetTargetServices() returns non-null if this is the sandbox target, and
  // null otherwise.
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  const bool is_sandbox_target = (sandbox_target_services != nullptr);

  base::string16 log_suffix =
      command_line->HasSwitch(chrome_cleaner::kElevatedSwitch)
          ? kElevatedLogFileSuffix
          : L"";
  log_suffix += is_sandbox_target ? chrome_cleaner::kSandboxLogFileSuffix : L"";

  // This has to be created after CrashReporterMain() above, as
  // CrashReporterMain creates its own ScopedLogging to upload logs.

  chrome_cleaner::ScopedLogging scoped_logging(log_suffix);

  // Only start the crash reporter for the main process, the sandboxed process
  // will use the same crash reporter.
  if (is_sandbox_target) {
    const base::string16 ipc_pipe_name = command_line->GetSwitchValueNative(
        chrome_cleaner::kUseCrashHandlerWithIdSwitch);
    CHECK(!ipc_pipe_name.empty());
    UseCrashReporter(ipc_pipe_name);
  } else {
    StartCrashReporter(CHROME_CLEANER_VERSION_UTF8_STRING);
  }

  const chrome_cleaner::Settings* settings =
      chrome_cleaner::Settings::GetInstance();

  if (settings->execution_mode() == ExecutionMode::kNone) {
    ::MessageBox(nullptr,
                 L"Manually running this program is no longer supported. "
                 L"Please visit "
                 L"https://support.google.com/chrome/?p=chrome_cleanup_tool "
                 L"for more information.",
                 L"Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
    return chrome_cleaner::RESULT_CODE_MANUAL_EXECUTION_BY_USER;
  }

  // Process priority modification has to be done before threads are created
  // because they inherit process' priority.
  if (settings->execution_mode() == ExecutionMode::kScanning) {
    chrome_cleaner::SetBackgroundMode();
  } else {
    if (!SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS))
      PLOG(ERROR) << "Can't SetPriorityClass to NORMAL_PRIORITY_CLASS";
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "chrome cleanup tool");

  chrome_cleaner::SandboxType sandbox_type =
      is_sandbox_target ? chrome_cleaner::SandboxProcessType()
                        : chrome_cleaner::SandboxType::kNonSandboxed;

  if (!chrome_cleaner::CrashClient::GetInstance()->InitializeCrashReporting(
          chrome_cleaner::CrashClient::Mode::CLEANER, sandbox_type)) {
    LOG(INFO) << "Crash reporting is not available.";
  } else {
    VLOG(1) << "Crash reporting initialized.";
  }

  if (is_sandbox_target) {
    switch (sandbox_type) {
      case chrome_cleaner::SandboxType::kParser:
        return chrome_cleaner::RunParserSandboxTarget(*command_line,
                                                      sandbox_target_services);
      case chrome_cleaner::SandboxType::kZipArchiver:
        return chrome_cleaner::RunZipArchiverSandboxTarget(
            *command_line, sandbox_target_services);
      case chrome_cleaner::SandboxType::kEngine:
        return RunEngineSandboxTarget(
            chrome_cleaner::CreateEngineDelegate(settings->engine()),
            *command_line, sandbox_target_services);
      default:
        NOTREACHED() << "Unknown sandbox type "
                     << static_cast<int>(sandbox_type);
    }
  }

  // Make sure we don't run two instances of the cleaner simultaneously
  // post-reboot.
  if (chrome_cleaner::Rebooter::IsPostReboot() &&
      chrome_cleaner::HasAdminRights()) {
    // The system closes the handle automatically when the process terminates,
    // and the event object is destroyed when its last handle has been closed.
    HANDLE event = ::CreateEvent(nullptr, FALSE, FALSE, L"chrome_cleanup_tool");
    if (event && ::GetLastError() == ERROR_ALREADY_EXISTS)
      return chrome_cleaner::RESULT_CODE_ALREADY_RUNNING;
  }

  // Setup Cleaner registry values.
  registry_logger.ClearExitCode();
  registry_logger.ClearEndTime();
  registry_logger.WriteVersion();
  registry_logger.WriteStartTime();

  // CoInitialize into the MTA since we desire to use the System Restore Point
  // API which requires we be in the MTA. Also needed for the task scheduler.
  base::win::ScopedCOMInitializer scoped_com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  bool succeeded = chrome_cleaner::InitializeCOMSecurity();
  PLOG_IF(ERROR, !succeeded) << "InitializeCOMSecurity() failed";
  DCHECK(succeeded);
  succeeded = chrome_cleaner::TaskScheduler::Initialize();
  LOG_IF(ERROR, !succeeded) << "TaskScheduler::Initialize() failed";
  DCHECK(succeeded);

  LOG(INFO) << "Command line arguments: "
            << chrome_cleaner::SanitizeCommandLine(*command_line);

  // Make sure that users won't be bothered again to confirm they want to run
  // the cleaner, especially post-reboot.
  base::FilePath executable_path =
      chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath();
  if (chrome_cleaner::HasZoneIdentifier(executable_path) &&
      !chrome_cleaner::OverwriteZoneIdentifier(executable_path)) {
    LOG(ERROR) << "Failed to remove zone identifier.";
  }

  // Many pieces of code below need a task executor to have been instantiated
  // before them.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  // The rebooter must be at the outermost scope so it can be called to reboot
  // before exiting, when appropriate.
  std::unique_ptr<chrome_cleaner::RebooterAPI> rebooter;

  if (command_line->HasSwitch(chrome_cleaner::kUploadLogFileSwitch)) {
    // Bail out of logs upload if upload is disabled.
    if (!settings->logs_upload_allowed()) {
      // Also get rid of all pending logs upload. Use a set to stop if we see
      // the same file name twice and make sure we don't go through some sort
      // of circular loop. Otherwise, we could spin forever if
      // GetNextLogFilePath returns the same file and never gets to return an
      // empty one. This might leave some log file behind, in very rare error
      // cases, but it's better than an infinite loop.
      std::set<base::string16> log_files;
      while (true) {
        base::FilePath log_file;
        registry_logger.GetNextLogFilePath(&log_file);
        if (log_file.empty() || !log_files.insert(log_file.value()).second)
          break;
        chrome_cleaner::PendingLogsService::ClearPendingLogFile(
            PRODUCT_SHORTNAME_STRING, log_file, &registry_logger);
      }

      return ReturnWithResultCode(
          chrome_cleaner::RESULT_CODE_EMPTY_CLIENT_ID_UPLOAD_ATTEMPT,
          executable_path, &registry_logger, nullptr);
    }

    succeeded = false;
    chrome_cleaner::PendingLogsService pending_logs_service;
    base::RunLoop run_loop;
    pending_logs_service.RetryNextPendingLogsUpload(
        PRODUCT_SHORTNAME_STRING,
        base::BindOnce(&LogsUploadCallback, &succeeded,
                       run_loop.QuitWhenIdleClosure()),
        &registry_logger);
    run_loop.Run();
    registry_logger.AppendLogUploadResult(succeeded);

    return ReturnWithResultCode(
        succeeded ? chrome_cleaner::RESULT_CODE_UPLOADED_PENDING_LOGS
                  : chrome_cleaner::RESULT_CODE_FAILED_TO_UPLOAD_LOGS,
        executable_path, &registry_logger, nullptr);
  }

  rebooter.reset(new chrome_cleaner::Rebooter(PRODUCT_SHORTNAME_STRING));

  shutdown_sequence.mojo_task_runner = chrome_cleaner::MojoTaskRunner::Create();

  // Only create the IPC if both the Mojo pipe token and the parent pipe handle
  // have been sent by Chrome. If either switch is not present, it will not be
  // connected to the parent process.
  // This pointer is leaked, in order to simplify this object's lifetime.
  chrome_cleaner::ChromePromptIPC* chrome_prompt_ipc = nullptr;

  if (settings->execution_mode() == ExecutionMode::kScanning) {
    // Scanning mode is only used by Chrome and all necessary IPC flags
    // must have been passed on the command line.
    if (!settings->switches_valid_for_ipc()) {
      return ReturnWithResultCode(
          chrome_cleaner::RESULT_CODE_INVALID_IPC_SWITCHES, executable_path,
          &registry_logger, rebooter.get());
    }

    if (settings->prompt_using_mojo()) {
      chrome_prompt_ipc = new chrome_cleaner::MojoChromePromptIPC(
          settings->chrome_mojo_pipe_token(),
          shutdown_sequence.mojo_task_runner);
    } else {
      // |chrome_prompt_ipc| takes ownership of the handles. The settings
      // object will still return the handle values when queried but from this
      // point on they may or may not be open.
      chrome_prompt_ipc = new chrome_cleaner::ProtoChromePromptIPC(
          base::win::ScopedHandle(settings->prompt_response_read_handle()),
          base::win::ScopedHandle(settings->prompt_request_write_handle()));
    }
  } else if (settings->has_any_ipc_switch()) {
    return ReturnWithResultCode(
        chrome_cleaner::RESULT_CODE_EXPECTED_SCANNING_EXECUTION_MODE,
        executable_path, &registry_logger, rebooter.get());
  }

  // If immediate elevation is not required, the process will restart elevated
  // after the user accepts to run cleanup.
  if (settings->execution_mode() != ExecutionMode::kScanning &&
      !chrome_cleaner::HasAdminRights()) {
    chrome_cleaner::ResultCode result_code = RelaunchElevated(&registry_logger);
    // If we failed to launch the elevated process, this process should call
    // ReturnWithResultCode to ensure all the properly post-run registry values
    // are written, and that all required cleanup is executed. If the elevated
    // process was created it will handle this.
    if (result_code != chrome_cleaner::RESULT_CODE_SUCCESS) {
      return ReturnWithResultCode(result_code, executable_path,
                                  &registry_logger, rebooter.get());
    }
    return result_code;
  }

  return ReturnWithResultCode(
      RunChromeCleaner(command_line, rebooter.get(), &registry_logger,
                       chrome_prompt_ipc, std::move(shutdown_sequence)),
      executable_path, &registry_logger, rebooter.get());
}
