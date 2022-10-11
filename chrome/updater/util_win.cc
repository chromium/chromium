// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

absl::optional<base::CommandLine> CommandLineForLegacyFormat(
    const std::wstring& cmd_string) {
  wchar_t** args = nullptr;
  int num_args = 0;
  args = ::CommandLineToArgvW(cmd_string.c_str(), &num_args);

  auto is_switch = [](const std::wstring& arg) { return arg[0] == L'-'; };

  auto is_legacy_switch = [](const std::wstring& arg) {
    return arg[0] == L'/';
  };

  // First argument is the program.
  base::CommandLine command_line(base::FilePath{args[0]});

  for (int i = 1; i < num_args; ++i) {
    const std::wstring next_arg = i < num_args - 1 ? args[i + 1] : L"";

    if (is_switch(args[i]) || is_switch(next_arg)) {
      // Won't parse Chromium-style command line.
      return absl::nullopt;
    }

    if (!is_legacy_switch(args[i])) {
      // This is a bare argument.
      command_line.AppendArg(base::WideToASCII(args[i]));
      continue;
    }

    const std::string switch_name = base::WideToASCII(&args[i][1]);
    if (switch_name.empty()) {
      VLOG(1) << "Empty switch in command line: [" << cmd_string << "]";
      return absl::nullopt;
    }

    if (is_legacy_switch(next_arg) || next_arg.empty()) {
      command_line.AppendSwitch(switch_name);
    } else {
      // Next argument is the value for this switch.
      command_line.AppendSwitchNative(switch_name, next_arg);
      ++i;
    }
  }

  return command_line;
}

absl::optional<base::FilePath> GetApplicationDataDirectory(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILES
                                  : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
  return app_data_dir;
}

absl::optional<base::FilePath> GetBaseInstallDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> app_data_dir =
      GetApplicationDataDirectory(scope);
  return app_data_dir ? app_data_dir->AppendASCII(COMPANY_SHORTNAME_STRING)
                            .AppendASCII(PRODUCT_FULLNAME_STRING)
                      : app_data_dir;
}

base::FilePath GetExecutableRelativePath() {
  return base::FilePath::FromASCII(kExecutableName);
}

}  // namespace updater
