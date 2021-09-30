// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

// Returns the KSAdmin exit code, and sets `std_out` to the contents of its
// stdout.
int RunKSAdmin(std::string* std_out, const std::vector<std::string>& args) {
  base::FilePath file_exe;
  EXPECT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));
  base::CommandLine command(
      file_exe.DirName().Append(FILE_PATH_LITERAL("ksadmin")));
  for (const auto& arg : args) {
    command.AppendSwitch(arg);
  }
  int exit_code = -1;
  base::GetAppOutputWithExitCode(command, std_out, &exit_code);
  return exit_code;
}

}  // namespace

TEST(KSAdminTest, ExitsOK) {
  std::string out;
  ASSERT_EQ(RunKSAdmin(&out, {}), 0);
  ASSERT_EQ(RunKSAdmin(&out, {"-H"}), 0);
  ASSERT_EQ(RunKSAdmin(&out, {"--unrecognized-argument", "value"}), 0);
}

}  // namespace updater
