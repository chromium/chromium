// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <psapi.h>

#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/software_reporter_tool_branding.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/crash/crash_client.h"
#include "chrome/chrome_cleaner/crash/crash_reporter.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/broker/interface_log_service.h"
#include "chrome/chrome_cleaner/engines/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/engines/controllers/scanner_controller_impl.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate_factory.h"
#include "chrome/chrome_cleaner/engines/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/executables/shutdown_sequence.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "chrome/chrome_cleaner/os/secure_dll_loading.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/shortcut_parser_api.h"
#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/scanner/scanner_controller.h"
#include "chrome/chrome_cleaner/settings/default_matching_options.h"
#include "chrome/chrome_cleaner/settings/engine_settings.h"
#include "chrome/chrome_cleaner/settings/matching_options.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "sandbox/win/src/sandbox_factory.h"

using chrome_cleaner::MojoTaskRunner;

namespace {

void WriteExitMetrics(chrome_cleaner::ResultCode result_code,
                      chrome_cleaner::RegistryLogger* registry_logger) {
  registry_logger->WriteExitCode(result_code);
  registry_logger->WriteEndTime();

  PROCESS_MEMORY_COUNTERS pmc;
  // TODO(joenotcharles): Log the total memory consumption instead of just the
  // main process'.
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    registry_logger->WriteMemoryUsage(pmc.PeakWorkingSetSize / 1024);
  }
}

chrome_cleaner::ResultCode FinalizeWithResultCode(
    chrome_cleaner::ResultCode result_code,
    chrome_cleaner::RegistryLogger* registry_logger) {
  chrome_cleaner::TaskScheduler::Terminate();
  LOG(INFO) << "Exiting with code: " << result_code;

  WriteExitMetrics(result_code, registry_logger);
  return result_code;
}

void TerminateOnSandboxConnectionError(
    const base::WeakPtr<chrome_cleaner::RegistryLogger>& registry_logger,
    const chrome_cleaner::SandboxType sandbox_type) {
  // If |registry_logger| has been deleted, the process is dying anyway, so no
  // action is needed.
  if (!registry_logger)
    return;

  chrome_cleaner::ResultCode result_code =
      chrome_cleaner::GetResultCodeForSandboxConnectionError(sandbox_type);
  WriteExitMetrics(result_code, registry_logger.get());
  chrome_cleaner::EarlyExit(result_code);
}

void CallTerminateOnSandboxConnectionError(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::WeakPtr<chrome_cleaner::RegistryLogger>& registry_logger,
    const chrome_cleaner::SandboxType sandbox_type) {
  // Weakptr has to be dereferenced on its factory thread.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(TerminateOnSandboxConnectionError,
                                       registry_logger, sandbox_type));
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
  // This must be executed as soon as possible to reduce the number of dlls that
  // the code might try to load before we can lock things down.
  chrome_cleaner::EnableSecureDllLoading();

  base::AtExitManager at_exit;

  // This must be done BEFORE constructing ScopedLogging, which calls
  // InitLogging to set the name of the log file, which needs to read
  // from the command line.
  bool success = base::CommandLine::Init(0, nullptr);
  DCHECK(success);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // If this process should run as the crash reporter, run that then return
  // immediately, as this process is not meant to be the reporter itself.
  if (command_line->HasSwitch(chrome_cleaner::kCrashHandlerSwitch))
    return CrashReporterMain();

  // GetTargetServices() returns non-null if this is the sandbox target, and
  // null otherwise.
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  const bool is_sandbox_target = (sandbox_target_services != nullptr);
  chrome_cleaner::ScopedLogging scoped_logging(
      is_sandbox_target ? chrome_cleaner::kSandboxLogFileSuffix : nullptr);

  // If there is a command line argument to add a registry suffix, set
  // the value for the registry_logger.
  const std::string registry_suffix =
      command_line->GetSwitchValueASCII(chrome_cleaner::kRegistrySuffixSwitch);

  chrome_cleaner::RegistryLogger registry_logger(
      chrome_cleaner::RegistryLogger::Mode::REPORTER, registry_suffix);

  // Weak pointer for connection error callback which may outlive the main. It
  // will be destroyed at the end of main before registry_logger.
  base::WeakPtrFactory<chrome_cleaner::RegistryLogger>
      registry_logger_weak_factory(&registry_logger);

  chrome_cleaner::ShutdownSequence shutdown_sequence;

  if (!chrome_cleaner::InitializeOSUtils()) {
    return FinalizeWithResultCode(
        chrome_cleaner::RESULT_CODE_INITIALIZATION_ERROR, &registry_logger);
  }

  // Start the crash handler only for the reporter process. The sandbox process
  // will use the handler process that was started by the reporter.
  if (command_line->HasSwitch(chrome_cleaner::kUseCrashHandlerWithIdSwitch)) {
    DCHECK(is_sandbox_target);
    const base::string16 ipc_pipe_name = command_line->GetSwitchValueNative(
        chrome_cleaner::kUseCrashHandlerWithIdSwitch);
    CHECK(!ipc_pipe_name.empty());
    UseCrashReporter(ipc_pipe_name);
  } else if (!is_sandbox_target) {
    // Start the crash reporter only if this is not the sandbox target. This is
    // the case for tests, where the |kUseCrashHandlerWithIdSwitch| switch is
    // not passed (and we don't want a crash reporter process to be started).
    StartCrashReporter(CHROME_CLEANER_VERSION_UTF8_STRING);
  }

  LOG(INFO) << "Command line arguments: "
            << chrome_cleaner::SanitizeCommandLine(*command_line);

  const chrome_cleaner::CrashClient::Mode crash_client_mode =
      chrome_cleaner::CrashClient::Mode::REPORTER;

  chrome_cleaner::SandboxType sandbox_type =
      is_sandbox_target ? chrome_cleaner::SandboxProcessType()
                        : chrome_cleaner::SandboxType::kNonSandboxed;

  if (!chrome_cleaner::CrashClient::GetInstance()->InitializeCrashReporting(
          crash_client_mode, sandbox_type)) {
    LOG(INFO) << "Crash reporting is not available.";
  } else {
    LOG(INFO) << "Crash reporting initialized.";
  }

  // Make sure not to take too much of the machines's resources.
  chrome_cleaner::SetBackgroundMode();

  const chrome_cleaner::Settings* settings =
      chrome_cleaner::Settings::GetInstance();

  if (is_sandbox_target) {
    switch (sandbox_type) {
      case chrome_cleaner::SandboxType::kEngine:
        return RunEngineSandboxTarget(
            chrome_cleaner::CreateEngineDelegate(settings->engine()),
            *command_line, sandbox_target_services);
      case chrome_cleaner::SandboxType::kParser:
        return chrome_cleaner::RunParserSandboxTarget(*command_line,
                                                      sandbox_target_services);
      default:
        NOTREACHED() << "Unknown sandbox type "
                     << static_cast<int>(sandbox_type);
    }
  }

  registry_logger.ClearScanTimes();
  registry_logger.ClearExitCode();
  registry_logger.WriteStartTime();

  if (!settings->scan_switches_correct()) {
    return FinalizeWithResultCode(
        chrome_cleaner::RESULT_CODE_INVALID_SCANNING_SWITCHES,
        &registry_logger);
  }

  // Many pieces of code below need a task executor to have been instantiated
  // before them.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "software reporter");
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  shutdown_sequence.mojo_task_runner = MojoTaskRunner::Create();

  if (!chrome_cleaner::IsSupportedEngine(settings->engine())) {
    LOG(FATAL) << "Unsupported engine " << settings->engine();
    return FinalizeWithResultCode(chrome_cleaner::RESULT_CODE_FAILED,
                                  &registry_logger);
  }

  chrome_cleaner::InitializePUPDataWithCatalog(settings->engine());

  base::string16 interface_log_file;
  if (command_line->HasSwitch(chrome_cleaner::kLogInterfaceCallsToSwitch)) {
    interface_log_file = command_line->GetSwitchValueNative(
        chrome_cleaner::kLogInterfaceCallsToSwitch);
    base::FilePath passed_name(interface_log_file);
    std::vector<base::string16> components;
    passed_name.GetComponents(&components);
    if (components.size() != 1) {
      LOG(ERROR) << "Invalid file name passed for logging!";
      return FinalizeWithResultCode(chrome_cleaner::RESULT_CODE_FAILED,
                                    &registry_logger);
    }
  }

  auto sandbox_connection_error_callback =
      base::BindRepeating(CallTerminateOnSandboxConnectionError,
                          base::SequencedTaskRunnerHandle::Get(),
                          registry_logger_weak_factory.GetWeakPtr());

  std::unique_ptr<chrome_cleaner::InterfaceLogService> interface_log_service =
      chrome_cleaner::InterfaceLogService::Create(
          interface_log_file, CHROME_CLEANER_VERSION_STRING);

  chrome_cleaner::ResultCode engine_result_code;
  std::tie(engine_result_code, shutdown_sequence.engine_client) =
      chrome_cleaner::SpawnEngineSandbox(settings->engine(), &registry_logger,
                                         shutdown_sequence.mojo_task_runner,
                                         sandbox_connection_error_callback,
                                         std::move(interface_log_service));
  if (engine_result_code != chrome_cleaner::RESULT_CODE_SUCCESS)
    return FinalizeWithResultCode(engine_result_code, &registry_logger);

  // CoInitialize into the MTA since we desire to use the task scheduler.
  base::win::ScopedCOMInitializer scoped_com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  bool succeeded = chrome_cleaner::TaskScheduler::Initialize();
  DCHECK(succeeded) << "TaskScheduler::Initialize() failed";

  // Initialize the sandbox for the shortcut parser.
  chrome_cleaner::RemoteParserPtr parser(nullptr,
                                         base::OnTaskRunnerDeleter(nullptr));
  chrome_cleaner::ResultCode parser_result_code =
      chrome_cleaner::SpawnParserSandbox(
          shutdown_sequence.mojo_task_runner.get(),
          sandbox_connection_error_callback, &parser);
  if (parser_result_code != chrome_cleaner::RESULT_CODE_SUCCESS)
    return FinalizeWithResultCode(parser_result_code, &registry_logger);
  std::unique_ptr<chrome_cleaner::ShortcutParserAPI> shortcut_parser =
      std::make_unique<chrome_cleaner::SandboxedShortcutParser>(
          shutdown_sequence.mojo_task_runner.get(), parser.get());

  std::unique_ptr<chrome_cleaner::ScannerController> scanner_controller =
      std::make_unique<chrome_cleaner::ScannerControllerImpl>(
          shutdown_sequence.engine_client.get(), &registry_logger,
          base::SequencedTaskRunnerHandle::Get(), shortcut_parser.get());

  if (command_line->HasSwitch(chrome_cleaner::kCrashSwitch)) {
    int* crash_me = nullptr;
    *crash_me = 42;
  }

  if (command_line->HasSwitch(chrome_cleaner::kLoadEmptyDLLSwitch)) {
    chrome_cleaner::testing::LoadEmptyDLL();
  }
  chrome_cleaner::NotifyInitializationDoneForTesting();

  auto result_code =
      static_cast<chrome_cleaner::ResultCode>(scanner_controller->ScanOnly());
  return FinalizeWithResultCode(result_code, &registry_logger);
}
