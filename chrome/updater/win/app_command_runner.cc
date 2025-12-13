// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/app_command_runner.h"

#include <windows.h>

#include <shellapi.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_impl_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_localalloc.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/event_history.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

// Loads the AppCommand under:
//     Update\Clients\{`app_id`}\Commands\`command_id`
//         REG_SZ "CommandLine" == {command format}
HRESULT LoadAppCommandFormat(UpdaterScope scope,
                             const std::wstring& app_id,
                             const std::wstring& command_id,
                             std::wstring& command_format) {
  base::win::RegKey command_key;
  HRESULT hr = HRESULT_FROM_WIN32(command_key.Open(
      UpdaterScopeToHKeyRoot(scope),
      GetAppCommandKey(app_id, command_id).c_str(), Wow6432(KEY_QUERY_VALUE)));
  return SUCCEEDED(hr) ? HRESULT_FROM_WIN32(command_key.ReadValue(
                             kRegValueCommandLine, &command_format))
                       : hr;
}

// Loads the ProcessLauncher command in HKLM under:
//     Update\Clients\{`app_id`}
//         REG_SZ `command_id` == {command format}
//
// The legacy process launcher format is only supported for Google Chrome
// versions 110.0.5435.0 and below with the "cmd" command id. This is because
// the legacy process launcher command layout format can be used to interpret
// and/or execute unrelated registry entries. For instance, if the app_id is
// `{8A69D345-D564-463c-AFF1-A69D9E530F96}`, the older command would be
// registered under
// `SOFTWARE\Google\Update\Clients\{8A69D345-D564-463c-AFF1-A69D9E530F96}`
// REG_SZ `cmd`. Along with `cmd`, there are other properties of the app
// registered, such as the version "pv"="107.0.5304.107". So, `pv` is also a
// potential "command" for `IProcessLauncher`, which is unexpected.
HRESULT LoadLegacyProcessLauncherFormat(const std::wstring& app_id,
                                        const std::wstring& command_id,
                                        std::wstring& command_format) {
  static constexpr wchar_t kAllowedLegacyProcessLauncherAppNamePrefix[] =
      L"" BROWSER_PRODUCT_NAME_STRING;
  static constexpr char kAllowedLegacyProcessLauncherMaxAppVersion[] =
      "110.0.5435.0";
  static constexpr wchar_t kAllowedLegacyProcessLauncherCommandId[] = L"cmd";

  std::wstring pv;
  std::wstring name;
  if (command_id == kAllowedLegacyProcessLauncherCommandId) {
    base::win::RegKey app_key;
    HRESULT hr = HRESULT_FROM_WIN32(
        app_key.Open(HKEY_LOCAL_MACHINE, GetAppClientsKey(app_id).c_str(),
                     Wow6432(KEY_QUERY_VALUE)));
    if (FAILED(hr)) {
      return hr;
    }

    app_key.ReadValue(kRegValuePV, &pv);
    app_key.ReadValue(kRegValueName, &name);
    const base::Version app_version(base::WideToUTF8(pv));

    if (app_version.IsValid() &&
        app_version.CompareTo(
            base::Version(kAllowedLegacyProcessLauncherMaxAppVersion)) <= 0 &&
        base::StartsWith(name, kAllowedLegacyProcessLauncherAppNamePrefix)) {
      return HRESULT_FROM_WIN32(
          app_key.ReadValue(command_id.c_str(), &command_format));
    }
  }

  LOG(WARNING)
      << __func__
      << "Legacy ProcessLauncher format not supported, use more secure "
         "AppCommand format: "
      << app_id << ": " << pv << ": " << name << ": " << command_id;
  return E_INVALIDARG;
}

bool IsParentOf(int key, const base::FilePath& child) {
  base::FilePath path;
  return base::PathService::Get(key, &path) && path.IsParent(child);
}

bool IsSecureAppCommandExePath(UpdaterScope scope,
                               const base::FilePath& exe_path) {
  return exe_path.IsAbsolute() &&
         (!IsSystemInstall(scope) ||
          IsParentOf(base::DIR_PROGRAM_FILES, exe_path) ||
          IsParentOf(base::DIR_PROGRAM_FILESX86, exe_path) ||
          IsParentOf(base::DIR_PROGRAM_FILES6432, exe_path));
}

}  // namespace

AppCommandRunner::AppCommandRunner(const std::wstring& app_id)
    : app_id_(app_id) {}
AppCommandRunner::~AppCommandRunner() = default;

// static
HResultOr<scoped_refptr<AppCommandRunner>> AppCommandRunner::LoadAppCommand(
    UpdaterScope scope,
    const std::wstring& app_id,
    const std::wstring& command_id) {
  std::wstring command_format;
  HRESULT hr = LoadAppCommandFormat(scope, app_id, command_id, command_format);
  if (FAILED(hr)) {
    if (IsSystemInstall(scope)) {
      hr = LoadLegacyProcessLauncherFormat(app_id, command_id, command_format);
    }
    if (FAILED(hr)) {
      return base::unexpected(hr);
    }
  }

  scoped_refptr<AppCommandRunner> app_command_runner =
      base::MakeRefCounted<AppCommandRunner>(app_id);
  hr = GetAppCommandFormatComponents(scope, command_format,
                                     app_command_runner->executable_,
                                     app_command_runner->parameters_);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  return app_command_runner;
}

// static
std::vector<scoped_refptr<AppCommandRunner>>
AppCommandRunner::LoadAutoRunOnOsUpgradeAppCommands(
    UpdaterScope scope,
    const std::wstring& app_id) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  const std::wstring commands_key_name = GetAppCommandKey(app_id, L"");

  std::vector<scoped_refptr<AppCommandRunner>> app_command_runners;
  for (base::win::RegistryKeyIterator it(root, commands_key_name.c_str(),
                                         KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const base::win::RegKey command_key(
        root, base::StrCat({commands_key_name, it.Name()}).c_str(),
        Wow6432(KEY_QUERY_VALUE));
    if (!command_key.Valid()) {
      continue;
    }

    DWORD auto_run = 0;
    if (command_key.ReadValueDW(kRegValueAutoRunOnOSUpgrade, &auto_run) !=
            ERROR_SUCCESS ||
        !auto_run) {
      continue;
    }

    HResultOr<scoped_refptr<AppCommandRunner>> runner =
        LoadAppCommand(scope, app_id, it.Name());
    if (runner.has_value()) {
      app_command_runners.push_back(*std::move(runner));
    }
  }

  return app_command_runners;
}

HRESULT AppCommandRunner::Run(base::span<const std::wstring> substitutions,
                              base::Process& process) {
  AppCommandStartEvent start_event;
  start_event.SetAppId(base::WideToUTF8(app_id_));

  if (executable_.empty()) {
    start_event.AddError({.code = E_UNEXPECTED})
        .WriteAsyncAndReturnEndEvent()
        .WriteAsync();
    return E_UNEXPECTED;
  }

  VLOG(2) << __func__ << ": " << executable_ << ": "
          << base::JoinString(parameters_, L",") << " : "
          << base::JoinString(substitutions, L",");

  const std::optional<std::wstring> command_line_parameters =
      FormatAppCommandLine(parameters_, substitutions);
  if (!command_line_parameters) {
    LOG(ERROR) << __func__ << "!command_line_parameters";
    start_event.AddError({.code = E_INVALIDARG})
        .WriteAsyncAndReturnEndEvent()
        .WriteAsync();
    return E_INVALIDARG;
  }
  const std::wstring command_line = base::StrCat(
      {base::CommandLine::QuoteForCommandLineToArgvW(executable_.value()), L" ",
       *command_line_parameters});
  VLOG(2) << __func__ << ": " << command_line;

  // `executable_` needs to be a full path to prevent `::CreateProcess` (which
  // `AppCommandRunner::LaunchProcess` uses internally) from using the search
  // path for path resolution.
  if (!executable_.IsAbsolute()) {
    LOG(ERROR) << __func__ << "!executable_.IsAbsolute(): " << executable_;
    start_event.AddError({.code = E_INVALIDARG})
        .WriteAsyncAndReturnEndEvent()
        .WriteAsync();
    return E_INVALIDARG;
  }

  {
    base::AutoLock lock{lock_};
    output_ = {};
  }
  command_completed_event_.Reset();

  // Holds the result of the IPC to retrieve the process and hr from
  // `GetAppOutputWithExitCodeAndTimeout`.
  struct GetAppOutputWithExitCodeAndTimeoutResult
      : public base::RefCountedThreadSafe<
            GetAppOutputWithExitCodeAndTimeoutResult> {
    std::optional<base::Process> process;
    std::optional<HRESULT> hr;
    std::optional<int> exit_code;
    base::WaitableEvent process_event;

   private:
    friend class base::RefCountedThreadSafe<
        GetAppOutputWithExitCodeAndTimeoutResult>;
    virtual ~GetAppOutputWithExitCodeAndTimeoutResult() = default;
  };

  AppCommandEndEvent end_event =
      start_event.SetCommandLine(base::WideToUTF8(command_line))
          .WriteAsyncAndReturnEndEvent();
  auto result =
      base::MakeRefCounted<GetAppOutputWithExitCodeAndTimeoutResult>();
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<AppCommandRunner> obj,
                 const std::wstring& command_line,
                 scoped_refptr<GetAppOutputWithExitCodeAndTimeoutResult> result)
                  -> HRESULT {
                base::LaunchOptions options = {};
                options.feedback_cursor_off = true;
                options.start_hidden = true;

                base::TerminationStatus final_status =
                    base::TerminationStatus::TERMINATION_STATUS_MAX_ENUM;

                int exit_code = -1;
                if (base::GetAppOutputWithExitCodeAndTimeout(
                        command_line,
                        /*include_stderr=*/true, nullptr, &exit_code,
                        kWaitForAppInstaller, options,
                        [&](const base::Process& process,
                            std::string_view partial_output) {
                          if (!result->process) {
                            result->process = process.Duplicate();
                            VLOG(1)
                                << "AppCommand pid: " << result->process->Pid();
                            result->process_event.Signal();
                          }

                          if (!partial_output.empty()) {
                            VLOG(1) << "AppCommand output: " << partial_output;
                            base::AutoLock lock{obj->lock_};
                            obj->output_ += partial_output;
                          }
                        },
                        &final_status)) {
                  result->exit_code = exit_code;
                }

                if (final_status ==
                    base::TerminationStatus::TERMINATION_STATUS_LAUNCH_FAILED) {
                  VLOG(1) << "AppCommand failed to launch";
                  return GOOPDATEINSTALL_E_INSTALLER_FAILED_START;
                }
                if (final_status ==
                    base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING) {
                  VLOG(1) << "AppCommand timed out";
                  return GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT;
                }

                return S_OK;
              },
              base::WrapRefCounted(this), command_line, result)
              .Then(base::BindOnce(
                  [](scoped_refptr<AppCommandRunner> obj,
                     scoped_refptr<GetAppOutputWithExitCodeAndTimeoutResult>
                         result,
                     AppCommandEndEvent end_event, HRESULT hr) {
                    result->hr = hr;
                    result->process_event.Signal();
                    obj->command_completed_event_.Signal();

                    if (FAILED(hr)) {
                      end_event.AddError({.code = hr});
                    }
                    if (result->exit_code) {
                      end_event.SetExitCode(*result->exit_code);
                    }
                    std::string command_output = obj->output();
                    if (!command_output.empty()) {
                      end_event.SetOutput(command_output);
                    }
                    end_event.WriteAsync();
                  },
                  base::WrapRefCounted(this), result, std::move(end_event))));

  result->process_event.Wait();

  if (!result->process) {
    HRESULT hr = result->hr.value_or(E_UNEXPECTED);
    LOG(ERROR) << __func__ << ": base::LaunchProcess failed: " << hr;
    return hr;
  }

  process = result->process->Duplicate();
  VLOG(2) << __func__ << ": Started process with PID: " << process.Pid();
  return S_OK;
}

std::string AppCommandRunner::output() {
  base::AutoLock lock{lock_};
  return output_;
}

bool AppCommandRunner::TimedWait(base::TimeDelta wait_delta) {
  return command_completed_event_.TimedWait(wait_delta);
}

// static
HRESULT AppCommandRunner::GetAppCommandFormatComponents(
    UpdaterScope scope,
    std::wstring command_format,
    base::FilePath& executable,
    std::vector<std::wstring>& parameters) {
  VLOG(2) << __func__ << ": " << scope << ": " << command_format;

  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> argv(
      ::CommandLineToArgvW(&command_format[0], &num_args));
  if (!argv || num_args < 1) {
    LOG(ERROR) << __func__ << "!argv || num_args < 1: " << num_args;
    return E_INVALIDARG;
  }

  // SAFETY: the unsafe buffer is present due to the ::CommandLineToArgvW call.
  // When constructing the span, `num_args` is validated and checked as a valid
  // size_t value.
  UNSAFE_BUFFERS(const base::span<wchar_t*> safe_args{
      argv.get(), base::checked_cast<size_t>(num_args)});

  const base::FilePath exe(safe_args[0]);
  if (!IsSecureAppCommandExePath(scope, exe)) {
    LOG(WARNING) << __func__
                 << ": !IsSecureAppCommandExePath(scope, exe): " << exe;
    return E_INVALIDARG;
  }

  executable = exe;
  parameters.clear();
  for (size_t i = 1; i < safe_args.size(); ++i) {
    parameters.push_back(safe_args[i]);
  }

  return S_OK;
}

// static
std::optional<std::wstring> AppCommandRunner::FormatParameter(
    const std::wstring& parameter,
    base::span<const std::wstring> substitutions) {
  return base::internal::DoReplaceStringPlaceholders(
      /*format_string=*/parameter, /*subst=*/substitutions,
      /*placeholder_prefix=*/L'%',
      /*should_escape_multiple_placeholder_prefixes=*/false,
      /*is_strict_mode=*/true, /*offsets=*/nullptr);
}

// static
std::optional<std::wstring> AppCommandRunner::FormatAppCommandLine(
    const std::vector<std::wstring>& parameters,
    base::span<const std::wstring> substitutions) {
  std::wstring formatted_command_line;
  for (size_t i = 0; i < parameters.size(); ++i) {
    std::optional<std::wstring> formatted_parameter =
        FormatParameter(parameters[i], substitutions);
    if (!formatted_parameter) {
      VLOG(1) << __func__ << " FormatParameter failed: " << parameters[i]
              << ": " << substitutions.size();
      return std::nullopt;
    }

    static constexpr wchar_t kQuotableCharacters[] = L" \t\\\"";
    formatted_command_line.append(
        formatted_parameter->find_first_of(kQuotableCharacters) ==
                std::wstring::npos
            ? *formatted_parameter  // no quoting needed, use as-is.
            : base::CommandLine::QuoteForCommandLineToArgvW(
                  *formatted_parameter));

    if (i + 1 < parameters.size()) {
      formatted_command_line.push_back(L' ');
    }
  }

  return formatted_command_line;
}

}  // namespace updater
