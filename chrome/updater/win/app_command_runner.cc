// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/win/app_command_runner.h"

#include <windows.h>

#include <shellapi.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_impl_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_localalloc.h"
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
  constexpr wchar_t kAllowedLegacyProcessLauncherAppNamePrefix[] =
      L"" BROWSER_PRODUCT_NAME_STRING;
  constexpr char kAllowedLegacyProcessLauncherMaxAppVersion[] = "110.0.5435.0";
  constexpr wchar_t kAllowedLegacyProcessLauncherCommandId[] = L"cmd";

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
    const base::Version app_version(base::WideToASCII(pv));

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

AppCommandRunner::AppCommandRunner() = default;
AppCommandRunner::AppCommandRunner(const AppCommandRunner&) = default;
AppCommandRunner& AppCommandRunner::operator=(const AppCommandRunner&) =
    default;
AppCommandRunner::~AppCommandRunner() = default;

// static
HResultOr<AppCommandRunner> AppCommandRunner::LoadAppCommand(
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

  AppCommandRunner app_command_runner;
  hr = GetAppCommandFormatComponents(scope, command_format,
                                     app_command_runner.executable_,
                                     app_command_runner.parameters_);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  return app_command_runner;
}

// static
std::vector<AppCommandRunner>
AppCommandRunner::LoadAutoRunOnOsUpgradeAppCommands(
    UpdaterScope scope,
    const std::wstring& app_id) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  const std::wstring commands_key_name = GetAppCommandKey(app_id, L"");

  std::vector<AppCommandRunner> app_command_runners;
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

    HResultOr<AppCommandRunner> runner =
        LoadAppCommand(scope, app_id, it.Name());
    if (runner.has_value()) {
      app_command_runners.push_back(*std::move(runner));
    }
  }

  return app_command_runners;
}

HRESULT AppCommandRunner::Run(const std::vector<std::wstring>& substitutions,
                              base::Process& process) const {
  if (executable_.empty() || process.IsValid()) {
    return E_UNEXPECTED;
  }

  return ExecuteAppCommand(executable_, parameters_, substitutions, process);
}

// static
HRESULT AppCommandRunner::StartProcess(const base::FilePath& executable,
                                       const std::wstring& parameters,
                                       base::Process& process) {
  VLOG(2) << __func__ << ": " << executable << ": " << parameters;

  if (executable.empty() || process.IsValid()) {
    return E_UNEXPECTED;
  }

  // `executable` needs to be a full path to prevent `::CreateProcess` (which
  // `base::LaunchProcess` uses internally) from using the search path for path
  // resolution.
  if (!executable.IsAbsolute()) {
    LOG(ERROR) << __func__ << "!executable.IsAbsolute(): " << executable;
    return E_INVALIDARG;
  }

  base::LaunchOptions options = {};
  options.feedback_cursor_off = true;
  options.start_hidden = true;

  process = base::LaunchProcess(
      base::StrCat(
          {base::CommandLine::QuoteForCommandLineToArgvW(executable.value()),
           L" ", parameters}),
      options);
  if (!process.IsValid()) {
    const HRESULT hr = HRESULTFromLastError();
    LOG(ERROR) << __func__ << "base::LaunchProcess failed: " << hr;
    return hr;
  }

  VLOG(2) << __func__ << "Started process with PID: " << process.Pid();
  return S_OK;
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

  const base::FilePath exe = base::FilePath(argv.get()[0]);
  if (!IsSecureAppCommandExePath(scope, exe)) {
    LOG(WARNING) << __func__
                 << ": !IsSecureAppCommandExePath(scope, exe): " << exe;
    return E_INVALIDARG;
  }

  executable = exe;
  parameters.clear();
  for (int i = 1; i < num_args; ++i) {
    parameters.push_back(argv.get()[i]);
  }

  return S_OK;
}

// static
std::optional<std::wstring> AppCommandRunner::FormatParameter(
    const std::wstring& parameter,
    const std::vector<std::wstring>& substitutions) {
  return base::internal::DoReplaceStringPlaceholders(
      /*format_string=*/parameter, /*subst=*/substitutions,
      /*placeholder_prefix=*/L'%',
      /*should_escape_multiple_placeholder_prefixes=*/false,
      /*is_strict_mode=*/true, /*offsets=*/nullptr);
}

// static
std::optional<std::wstring> AppCommandRunner::FormatAppCommandLine(
    const std::vector<std::wstring>& parameters,
    const std::vector<std::wstring>& substitutions) {
  std::wstring formatted_command_line;
  for (size_t i = 0; i < parameters.size(); ++i) {
    std::optional<std::wstring> formatted_parameter =
        FormatParameter(parameters[i], substitutions);
    if (!formatted_parameter) {
      VLOG(1) << __func__ << " FormatParameter failed: " << parameters[i]
              << ": " << substitutions.size();
      return std::nullopt;
    }

    constexpr wchar_t kQuotableCharacters[] = L" \t\\\"";
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

// static
HRESULT AppCommandRunner::ExecuteAppCommand(
    const base::FilePath& executable,
    const std::vector<std::wstring>& parameters,
    const std::vector<std::wstring>& substitutions,
    base::Process& process) {
  VLOG(2) << __func__ << ": " << executable << ": "
          << base::JoinString(parameters, L",") << " : "
          << base::JoinString(substitutions, L",");

  const std::optional<std::wstring> command_line_parameters =
      FormatAppCommandLine(parameters, substitutions);
  if (!command_line_parameters) {
    LOG(ERROR) << __func__ << "!command_line_parameters";
    return E_INVALIDARG;
  }

  return StartProcess(executable, command_line_parameters.value(), process);
}

}  // namespace updater
