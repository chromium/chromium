// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file provides a command-line interface to
// upgrade_test::GenerateAlternateVersion().

#include <stddef.h>

#include <cstdio>
#include <cstdlib>

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/installer/test/alternate_version_generator.h"

namespace {

const wchar_t kDefaultMiniInstallerFile[] = L"mini_installer.exe";
const wchar_t kDefaultOutPath[] = L"mini_installer_new.exe";

namespace switches {

const char kForce[] = "force";
const char kHelp[] = "help";
const char kMiniInstaller[] = "mini_installer";
const char kOut[] = "out";
const char kPrevious[] = "previous";

}  // namespace switches

namespace errors {

enum ErrorCode {
  SHOW_HELP,
  MINI_INSTALLER_NOT_FOUND,
  OUT_FILE_EXISTS,
  GENERATION_FAILED
};

const wchar_t* const Messages[] = {
    nullptr, L"original mini_installer.exe not found",
    L"output file already exists",
    L"failed to generate a newly versioned mini_installer.exe"};

const wchar_t* GetErrorMessage(ErrorCode error_code) {
  DCHECK_LE(0, error_code);
  DCHECK_GT(std::size(Messages), static_cast<size_t>(error_code));
  return Messages[error_code];
}

}  // namespace errors

// Display usage information to stderr along with an optional error message with
// details.
void DumpUsage(const base::CommandLine& cmd_line,
               errors::ErrorCode error_code,
               const std::wstring& detail) {
  const wchar_t* error_message = errors::GetErrorMessage(error_code);
  if (error_message != nullptr) {
    fwprintf(stderr, L"%s: %s", cmd_line.GetProgram().value().c_str(),
             errors::GetErrorMessage(error_code));
    if (!detail.empty())
      fwprintf(stderr, L" (%s)\n", detail.c_str());
    else
      fwprintf(stderr, L"\n");
  }

  fwprintf(
      stderr,
      L"Usage: %s [ OPTIONS ]\n"
      L" Where OPTIONS is one or more of:\n"
      L" --help                     Display this help message.\n"
      L" --force                    Overwrite any existing output files.\n"
      L" --mini_installer=SRC_PATH  Path to mini_installer.exe.  Default value "
      L"is\n"
      L"                            \"mini_installer.exe\" in the same "
      L"directory as\n"
      L"                            this program.\n"
      L" --out=OUT_PATH             Path to output file. Default value is\n"
      L"                            \"mini_installer_new.exe\" in the current\n"
      L"                            directory.\n"
      L" --previous                 OUT_PATH will have a lower version than\n"
      L"                            SRC_PATH.  By default, OUT_PATH will have "
      L"a\n"
      L"                            higher version.\n"
      L" --7za_path=7ZA_PATH        Path to the directory holding 7za.exe. "
      L"Defaults\n"
      L"                            to "
      L"..\\..\\third_party\\lzma_sdk\\Executable\n"
      L"                            relative to this program's location.\n",
      cmd_line.GetProgram().value().c_str());
}

// Gets the path to the source mini_installer.exe on which to operate, putting
// the result in |mini_installer|.  Returns true on success.
bool GetMiniInstallerPath(const base::CommandLine& cmd_line,
                          base::FilePath* mini_installer) {
  DCHECK(mini_installer);
  base::FilePath result = cmd_line.GetSwitchValuePath(switches::kMiniInstaller);
  if (result.empty() && base::PathService::Get(base::DIR_EXE, &result))
    result = result.Append(kDefaultMiniInstallerFile);
  if (result.empty())
    return false;
  *mini_installer = result;
  return true;
}

// Gets the path to the output file, putting the result in |out|.
void GetOutPath(const base::CommandLine& cmd_line, base::FilePath* out) {
  DCHECK(out);
  base::FilePath result = cmd_line.GetSwitchValuePath(switches::kOut);
  if (result.empty())
    *out = base::FilePath(kDefaultOutPath);
  else
    *out = result;
}

// Returns the direction in which the version should be adjusted.
upgrade_test::Direction GetDirection(const base::CommandLine& cmd_line) {
  return cmd_line.HasSwitch(switches::kPrevious)
             ? upgrade_test::PREVIOUS_VERSION
             : upgrade_test::NEXT_VERSION;
}

}  // namespace

// The main program.
int wmain(int argc, wchar_t* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (cmd_line->HasSwitch(switches::kHelp)) {
    DumpUsage(*cmd_line, errors::SHOW_HELP, std::wstring());
    return EXIT_SUCCESS;
  }

  base::FilePath mini_installer;
  if (!GetMiniInstallerPath(*cmd_line, &mini_installer)) {
    DumpUsage(*cmd_line, errors::MINI_INSTALLER_NOT_FOUND, std::wstring());
    return EXIT_FAILURE;
  }

  if (!base::PathExists(mini_installer)) {
    DumpUsage(*cmd_line, errors::MINI_INSTALLER_NOT_FOUND,
              mini_installer.value());
    return EXIT_FAILURE;
  }

  base::FilePath out;
  GetOutPath(*cmd_line, &out);
  if (!cmd_line->HasSwitch(switches::kForce) && base::PathExists(out)) {
    DumpUsage(*cmd_line, errors::OUT_FILE_EXISTS, out.value());
    return EXIT_FAILURE;
  }

  upgrade_test::Direction direction = GetDirection(*cmd_line);

  std::wstring original_version;
  std::wstring new_version;

  if (upgrade_test::GenerateAlternateVersion(
          base::MakeAbsoluteFilePath(mini_installer),
          base::MakeAbsoluteFilePath(out), direction, &original_version,
          &new_version)) {
    fwprintf(stdout, L"Generated version %s from version %s\n",
             new_version.c_str(), original_version.c_str());
    return EXIT_SUCCESS;
  }

  DumpUsage(*cmd_line, errors::GENERATION_FAILED, mini_installer.value());
  return EXIT_FAILURE;
}
