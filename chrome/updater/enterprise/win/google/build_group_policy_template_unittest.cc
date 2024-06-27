// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(BuildGroupPolicyTemplateTest, AdmxFilesEqual) {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  base::CommandLine command(
      test_data_root.AppendASCII("chrome")
          .AppendASCII("updater")
          .AppendASCII("enterprise")
          .AppendASCII("win")
          .AppendASCII("google")
          .AppendASCII("build_group_policy_template_unittest.py"));
  command.AppendSwitchPath("--test_gold_admx_file",
                           test::GetTestFilePath("enterprise")
                               .AppendASCII("win")
                               .AppendASCII("google")
                               .AppendASCII("test_gold.admx"));
  command.AppendSwitchPath("--test_gold_adml_file",
                           test::GetTestFilePath("enterprise")
                               .AppendASCII("win")
                               .AppendASCII("google")
                               .AppendASCII("test_gold.adml"));
  base::FilePath output_path;
  ASSERT_TRUE(base::GetTempDir(&output_path));
  command.AppendSwitchPath("--output_path", output_path);
  EXPECT_EQ(test::RunVPythonCommand(command), 0);
  EXPECT_TRUE(base::DeletePathRecursively(output_path));
}

}  // namespace updater
