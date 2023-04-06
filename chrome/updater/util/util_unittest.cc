// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/test_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

TEST(Util, AppArgsAndAP) {
  base::test::ScopedCommandLine original_command_line;
  {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(kTagSwitch,
                                    "appguid=8a69f345-c564-463c-aff1-"
                                    "a69d9e530f96&appname=TestApp&ap=TestAP");

    // Test GetAppArgs.
    EXPECT_EQ(GetAppArgs("NonExistentAppId"), absl::nullopt);
    absl::optional<tagging::AppArgs> app_args =
        GetAppArgs("8a69f345-c564-463c-aff1-a69d9e530f96");
    ASSERT_NE(app_args, absl::nullopt);
    EXPECT_STREQ(app_args->app_id.c_str(),
                 "8a69f345-c564-463c-aff1-a69d9e530f96");
    EXPECT_STREQ(app_args->app_name.c_str(), "TestApp");
  }
}

TEST(Util, WriteInstallerDataToTempFile) {
  base::FilePath directory;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &directory));

  EXPECT_FALSE(WriteInstallerDataToTempFile(directory, ""));

  const std::string kInstallerData =
      R"({"distribution":{"msi":true,"allow_downgrade":false}})";
  EXPECT_FALSE(WriteInstallerDataToTempFile(
      directory.Append(FILE_PATH_LITERAL("NonExistentDirectory")),
      kInstallerData));

  const absl::optional<base::FilePath> installer_data_file =
      WriteInstallerDataToTempFile(directory, kInstallerData);
  ASSERT_TRUE(installer_data_file);

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(*installer_data_file, &contents));
  EXPECT_EQ(base::StrCat({kUTF8BOM, kInstallerData}), contents);

  EXPECT_TRUE(base::DeleteFile(*installer_data_file));
}

TEST(Util, GetTagArgsForCommandLine) {
  base::CommandLine command_line(base::FilePath(FILE_PATH_LITERAL("my.exe")));
  command_line.AppendSwitchASCII(kHandoffSwitch,
                                 "appguid={8a69}&appname=Chrome");
  command_line.AppendSwitchASCII(kAppArgsSwitch,
                                 "&appguid={8a69}&installerdata=%7B%22homepage%"
                                 "22%3A%22http%3A%2F%2Fwww.google.com%");
  command_line.AppendSwitch(kSilentSwitch);
  command_line.AppendSwitchASCII(kSessionIdSwitch, "{123-456}");

  TagParsingResult result = GetTagArgsForCommandLine(command_line);
  EXPECT_EQ(result.error, tagging::ErrorCode::kSuccess);
  EXPECT_EQ(result.tag_args->apps.size(), size_t{1});
  EXPECT_EQ(result.tag_args->apps[0].app_id, "{8a69}");
  EXPECT_EQ(result.tag_args->apps[0].app_name, "Chrome");
  EXPECT_EQ(result.tag_args->apps[0].encoded_installer_data,
            "%7B%22homepage%22%3A%22http%3A%2F%2Fwww.google.com%");
}

TEST(Util, GetCrashDatabasePath) {
  absl::optional<base::FilePath> crash_database_path(
      GetCrashDatabasePath(GetTestScope()));
  ASSERT_TRUE(crash_database_path);
  EXPECT_EQ(crash_database_path->BaseName().value(),
            FILE_PATH_LITERAL("Crashpad"));
}

}  // namespace updater
