// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
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
  const base::FilePath test_enterprise_dir = test::GetTestFilePath("enterprise")
                                                 .AppendASCII("win")
                                                 .AppendASCII("google");
  command.AppendSwitchPath("--test_gold_adm_file",
                           test_enterprise_dir.AppendASCII("test_gold.adm"));
  command.AppendSwitchPath("--test_gold_admx_file",
                           test_enterprise_dir.AppendASCII("test_gold.admx"));
  command.AppendSwitchPath("--test_gold_adml_file",
                           test_enterprise_dir.AppendASCII("test_gold.adml"));
  base::ScopedTempDir output_path;
  ASSERT_TRUE(output_path.CreateUniqueTempDir());
  command.AppendSwitchPath("--output_path", output_path.GetPath());
  EXPECT_EQ(test::RunVPythonCommand(command), 0)
      << "command: " << command.GetCommandLineString();
}

}  // namespace updater
