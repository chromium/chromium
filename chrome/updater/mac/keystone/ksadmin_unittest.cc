// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/keystone/ksadmin.h"

#include <map>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

// Returns the KSAdmin exit code, and sets `std_out` to the contents of its
// stdout.
int RunKSAdmin(std::string* std_out, const std::vector<std::string>& args) {
  base::FilePath out_dir;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  base::CommandLine command(out_dir.Append(FILE_PATH_LITERAL("ksadmin_test")));
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

TEST(KSAdminTest, PrintVersion) {
  std::string out;
  ASSERT_EQ(RunKSAdmin(&out, {"--ksadmin-version"}), 0);
  ASSERT_EQ(out, base::StrCat({kUpdaterVersion, "\n"}));
  out.clear();
  ASSERT_EQ(RunKSAdmin(&out, {"-k"}), 0);
  ASSERT_EQ(out, base::StrCat({kUpdaterVersion, "\n"}));
}

TEST(KSAdminTest, ParseCommandLine) {
  const char* argv[] = {"ksadmin",  "--register",
                        "-P",       "com.google.kipple",
                        "-v",       "1.2.3.4",
                        "--xcpath", "/Applications/GoogleKipple.app",
                        "-u",       "https://tools.google.com/service/update2"};

  std::map<std::string, std::string> arg_map =
      ParseCommandLine(std::size(argv), argv);
  EXPECT_EQ(arg_map.size(), size_t{5});
  EXPECT_EQ(arg_map.count("register"), size_t{1});
  EXPECT_EQ(arg_map["register"], "");
  EXPECT_EQ(arg_map["P"], "com.google.kipple");
  EXPECT_EQ(arg_map["v"], "1.2.3.4");
  EXPECT_EQ(arg_map["xcpath"], "/Applications/GoogleKipple.app");
  EXPECT_EQ(arg_map["u"], "https://tools.google.com/service/update2");
}

TEST(KSAdminTest, ParseCommandLine_DiffByCase) {
  const char* argv[] = {"ksadmin", "-k", "-K", "Tag"};

  std::map<std::string, std::string> arg_map =
      ParseCommandLine(std::size(argv), argv);
  EXPECT_EQ(arg_map.size(), size_t{2});
  EXPECT_EQ(arg_map.count("k"), size_t{1});
  EXPECT_EQ(arg_map["k"], "");
  EXPECT_EQ(arg_map["K"], "Tag");
}

TEST(KSAdminTest, ParseCommandLine_CombinedShortOptions) {
  const char* argv[] = {"ksadmin", "-pP", "com.google.Chrome"};

  std::map<std::string, std::string> arg_map =
      ParseCommandLine(std::size(argv), argv);
  EXPECT_EQ(arg_map.size(), size_t{2});
  EXPECT_EQ(arg_map.count("p"), size_t{1});
  EXPECT_EQ(arg_map["p"], "");
  EXPECT_EQ(arg_map["P"], "com.google.Chrome");
}

}  // namespace updater
