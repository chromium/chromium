// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#define EXECUTABLE_EXTENSION ".exe"
#else
#define EXECUTABLE_EXTENSION ".app"
#endif

// Tests the updater process returns 0 when run with --test argument.
TEST(UpdaterTest, UpdaterExitCode) {
  base::FilePath this_executable_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &this_executable_path));
  const base::FilePath executableFolder = this_executable_path.DirName();
  const base::FilePath updater =
#if defined(OS_WIN)
      this_executable_path.DirName().Append(
          FILE_PATH_LITERAL("updater" EXECUTABLE_EXTENSION));
#elif defined(OS_MAC)
      this_executable_path.DirName()
          .Append(
              FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING EXECUTABLE_EXTENSION))
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("MacOS"))
          .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING));
#else
      "";
#endif
  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  base::CommandLine command_line(updater);
  command_line.AppendSwitch("test");
  auto process = base::LaunchProcess(command_line, options);
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(60),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}
