// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"

namespace updater {
namespace {
constexpr base::FilePath::CharType kZipExePath[] =
    FILE_PATH_LITERAL("/usr/bin/unzip");
}

bool UnzipWithExe(const base::FilePath& src_path,
                  const base::FilePath& dest_path) {
  base::FilePath fp(kZipExePath);
  base::CommandLine command(fp);
  command.AppendArg(src_path.value());
  command.AppendArg("-d");
  command.AppendArg(dest_path.value());

  std::string output;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command, &output, &exit_code)) {
    VLOG(0) << "Something went wrong while running the unzipping with "
            << kZipExePath;
    return false;
  }

  // Unzip utility having 0 is success and 1 is a warning.
  if (exit_code > 1) {
    VLOG(0) << "Output from unzipping: " << output;
    VLOG(0) << "Exit code: " << exit_code;
  }

  return exit_code <= 1;
}

}  // namespace updater
