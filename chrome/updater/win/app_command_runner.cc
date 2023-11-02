// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/app_command_runner.h"

#include <shellapi.h>
#include <windows.h>

#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

// Formats a single `parameter` and returns the result. Any placeholder `%N` in
// `parameter` is replaced with substitutions[N - 1]. Any literal `%` needs to
// be escaped with a `%`.
//
// Returns `absl::nullopt` if:
// * a placeholder %N is encountered where N > substitutions.size().
// * a literal `%` is not escaped with a `%`.
//
// See examples in the WinUtil*FormatAppCommandLine unit tests.
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
// See examples in the WinUtil*FormatAppCommandLine unit tests.
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

bool IsSecureAppCommandExePath(UpdaterScope scope,
                               const base::FilePath& exe_path) {
  return exe_path.IsAbsolute() &&
         (scope == UpdaterScope::kUser ||
          IsParentOf(base::DIR_PROGRAM_FILESX86, exe_path) ||
          IsParentOf(base::DIR_PROGRAM_FILES6432, exe_path));
}

}  // namespace

AppCommandRunner::AppCommandRunner() = default;
AppCommandRunner::AppCommandRunner(const AppCommandRunner&) = default;
AppCommandRunner& AppCommandRunner::operator=(const AppCommandRunner&) =
    default;
AppCommandRunner::~AppCommandRunner() = default;

HRESULT AppCommandRunner::LoadAppCommand(UpdaterScope scope,
                                         const std::wstring& app_id,
                                         const std::wstring& command_id,
                                         AppCommandRunner& app_command_runner) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  std::wstring command_format;

  if (const base::win::RegKey command_key(
          root, GetAppCommandKey(app_id, command_id).c_str(),
          Wow6432(KEY_QUERY_VALUE));
      !command_key.Valid()) {
    const base::win::RegKey app_key(root, GetAppClientsKey(app_id).c_str(),
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

  return GetAppCommandFormatComponents(scope, command_format,
                                       app_command_runner.executable_,
                                       app_command_runner.parameters_);
}

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
    if (!command_key.Valid())
      continue;

    DWORD auto_run = 0;
    if (command_key.ReadValueDW(kRegValueAutoRunOnOSUpgrade, &auto_run) !=
            ERROR_SUCCESS ||
        !auto_run) {
      continue;
    }

    AppCommandRunner runner;
    if (SUCCEEDED(LoadAppCommand(scope, app_id, it.Name(), runner)))
      app_command_runners.push_back(runner);
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

HRESULT AppCommandRunner::StartProcess(const base::FilePath& executable,
                                       const std::wstring& command_line,
                                       base::Process& process) {
  if (executable.empty() || process.IsValid()) {
    return E_UNEXPECTED;
  }

  if (!executable.IsAbsolute())
    return E_INVALIDARG;

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  std::wstring parameters = command_line;

  // In contrast to the following call to `::CreateProcess`,
  // `base::Process::LaunchProcess` passes the `executable` in the
  // `lpCommandLine` parameter to `::CreateProcess`, which uses the search path
  // for path resolution of `executable`.
  if (!::CreateProcess(executable.value().c_str(), &parameters[0], nullptr,
                       nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                       &pi)) {
    return HRESULTFromLastError();
  }

  ::CloseHandle(pi.hThread);

  process = base::Process(pi.hProcess);
  CHECK(process.IsValid());
  return S_OK;
}

HRESULT AppCommandRunner::GetAppCommandFormatComponents(
    UpdaterScope scope,
    std::wstring command_format,
    base::FilePath& executable,
    std::vector<std::wstring>& parameters) {
  int num_args = 0;
  ScopedLocalAlloc args(::CommandLineToArgvW(&command_format[0], &num_args));
  if (!args.is_valid() || num_args < 1)
    return E_INVALIDARG;

  const wchar_t** argv = reinterpret_cast<const wchar_t**>(args.get());
  const base::FilePath exe = base::FilePath(argv[0]);
  if (!IsSecureAppCommandExePath(scope, exe))
    return E_INVALIDARG;

  executable = exe;
  parameters.clear();
  for (int i = 1; i < num_args; ++i)
    parameters.push_back(argv[i]);

  return S_OK;
}

absl::optional<std::wstring> AppCommandRunner::FormatAppCommandLine(
    const std::vector<std::wstring>& parameters,
    const std::vector<std::wstring>& substitutions) {
  std::wstring formatted_command_line;
  for (size_t i = 0; i < parameters.size(); ++i) {
    absl::optional<std::wstring> formatted_parameter =
        FormatParameter(substitutions, parameters[i]);
    if (!formatted_parameter) {
      LOG(ERROR) << __func__ << " FormatParameter failed: " << parameters[i]
                 << ": " << substitutions.size();
      return absl::nullopt;
    }

    formatted_command_line.append(
        QuoteForCommandLineToArgvW(*formatted_parameter));

    if (i + 1 < parameters.size())
      formatted_command_line.push_back(L' ');
  }

  return formatted_command_line;
}

HRESULT AppCommandRunner::ExecuteAppCommand(
    const base::FilePath& executable,
    const std::vector<std::wstring>& parameters,
    const std::vector<std::wstring>& substitutions,
    base::Process& process) {
  const absl::optional<std::wstring> command_line =
      FormatAppCommandLine(parameters, substitutions);
  if (!command_line)
    return E_INVALIDARG;

  return StartProcess(executable, command_line.value(), process);
}

}  // namespace updater
