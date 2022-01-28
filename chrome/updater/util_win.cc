// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

std::string GetSwitchValueInLegacyFormat(const std::wstring& command_line,
                                         const std::wstring& switch_name) {
  wchar_t** args = nullptr;
  int num_args = 0;
  args = ::CommandLineToArgvW(command_line.c_str(), &num_args);

  auto is_switch = [](const std::wstring& arg) {
    return !arg.empty() && arg.at(0) == u'/';
  };
  // For switch to have a value in legacy format, the switch name cannot be
  // the last argument.
  const int last_arg_to_check = num_args - 1;
  for (int i = 0; i < last_arg_to_check; ++i) {
    const std::wstring current_switch(args[i]);
    if (base::EqualsCaseInsensitiveASCII(current_switch,
                                         base::StrCat({L"/", switch_name})) &&
        !is_switch(args[i + 1])) {
      return base::WideToASCII(args[i + 1]);
    }
  }

  return std::string();
}

absl::optional<base::FilePath> GetUpdaterFolderPath(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILES
                                  : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

// TODO(crbug.com/1241276) combine "updater.exe" literals.
base::FilePath GetExecutableRelativePath() {
  return base::FilePath(FILE_PATH_LITERAL("updater.exe"));
}

}  // namespace updater
