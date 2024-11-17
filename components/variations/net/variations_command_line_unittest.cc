// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_command_line.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/variations_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(VariationsCommandLineTest, TestGetVariationsCommandLine) {
  std::string trial_list = "trial1/group1/*trial2/group2";
  std::string param_list = "trial1.group1:p1/v1/p2/2";
  std::string enable_feature_list = "feature1<trial1";
  std::string disable_feature_list = "feature2<trial2";

  AssociateParamsFromString(param_list);
  base::FieldTrialList::CreateTrialsFromString(trial_list);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(enable_feature_list,
                                          disable_feature_list);

  std::string output = VariationsCommandLine::GetForCurrentProcess().ToString();
  EXPECT_NE(output.find(trial_list), std::string::npos);
  EXPECT_NE(output.find(param_list), std::string::npos);
  EXPECT_NE(output.find(enable_feature_list), std::string::npos);
  EXPECT_NE(output.find(disable_feature_list), std::string::npos);
}

TEST(VariationsCommandLineTest, WriteReadToString_Normal) {
  VariationsCommandLine vc1;
  vc1.field_trial_states = "trial1/group1/*trial2/group2";
  vc1.field_trial_params = "trial1.group1:p1/v1/p2/2";
  vc1.enable_features = "feature1<trial1";
  vc1.disable_features = "feature2<trial2";
  std::string content;
  ASSERT_TRUE(vc1.WriteToString(&content));
  auto optional_vc = VariationsCommandLine::ReadFromString(content);
  ASSERT_TRUE(optional_vc.has_value());
  ASSERT_EQ(vc1.field_trial_states, optional_vc->field_trial_states);
  ASSERT_EQ(vc1.field_trial_params, optional_vc->field_trial_params);
  ASSERT_EQ(vc1.enable_features, optional_vc->enable_features);
  ASSERT_EQ(vc1.disable_features, optional_vc->disable_features);
}

const char* TEST_VARIATIONS =
    R"({
"force-fieldtrials":"*A/B/*C/D",
"force-fieldtrial-params":"P1:P2",
"enable-features":"F1<F1,F2<F2",
"disable-features":"F3<F3,F4<F4"
})";

TEST(VariationsCommandLineTest, MaybeUnpackVariationsStateFile_Encoded) {
  base::FilePath fp = base::CreateUniqueTempDirectoryScopedToTest();
  base::FilePath temp_file;
  CreateAndOpenTemporaryFileInDir(fp, &temp_file);
  std::string encoded = base::Base64Encode(TEST_VARIATIONS);
  CHECK(base::WriteFile(temp_file, encoded));
  base::test::ScopedCommandLine scoped_cmdline;
  base::CommandLine* cmdline = scoped_cmdline.GetProcessCommandLine();
  cmdline->AppendSwitchPath(variations::switches::kVariationsStateFile,
                            temp_file);
  MaybeUnpackVariationsStateFile();

  ASSERT_TRUE(cmdline->HasSwitch(::switches::kForceFieldTrials));
  std::string value =
      cmdline->GetSwitchValueASCII(::switches::kForceFieldTrials);
  ASSERT_EQ(value, "*A/B/*C/D");

  ASSERT_TRUE(cmdline->HasSwitch(variations::switches::kForceFieldTrialParams));
  value = cmdline->GetSwitchValueASCII(
      variations::switches::kForceFieldTrialParams);
  ASSERT_EQ(value, "P1:P2");

  ASSERT_TRUE(cmdline->HasSwitch(::switches::kEnableFeatures));
  value = cmdline->GetSwitchValueASCII(::switches::kEnableFeatures);
  ASSERT_EQ(value, "F1<F1,F2<F2");

  ASSERT_TRUE(cmdline->HasSwitch(::switches::kDisableFeatures));
  value = cmdline->GetSwitchValueASCII(::switches::kDisableFeatures);
  ASSERT_EQ(value, "F3<F3,F4<F4");

  ASSERT_FALSE(cmdline->HasSwitch(variations::switches::kVariationsStateFile));
}

TEST(VariationsCommandLineTest, MaybeUnpackVariationsStateFile_MixFieldTrial) {
  base::test::ScopedCommandLine scoped_cmdline;
  base::CommandLine* cmdline = scoped_cmdline.GetProcessCommandLine();
  cmdline->AppendSwitchASCII(variations::switches::kVariationsStateFile,
                             "file.txt");
  cmdline->AppendSwitchASCII(::switches::kForceFieldTrials, "fieldtrail");
  BASE_EXPECT_DEATH(MaybeUnpackVariationsStateFile(), "");
}

TEST(VariationsCommandLineTest, MaybeUnpackVariationsStateFile_FileNonExist) {
  base::test::ScopedCommandLine scoped_cmdline;
  base::CommandLine* cmdline = scoped_cmdline.GetProcessCommandLine();
  cmdline->AppendSwitchASCII(variations::switches::kVariationsStateFile,
                             "non_exist_file");
  BASE_EXPECT_DEATH(MaybeUnpackVariationsStateFile(), "");
}

TEST(VariationsCommandLineTest,
     MaybeUnpackVariationsStateFile_Base64DecodeFail) {
  base::FilePath fp = base::CreateUniqueTempDirectoryScopedToTest();
  base::FilePath temp_file;
  CreateAndOpenTemporaryFileInDir(fp, &temp_file);
  CHECK(base::WriteFile(temp_file, "invalid base64 string"));
  base::test::ScopedCommandLine scoped_cmdline;
  base::CommandLine* cmdline = scoped_cmdline.GetProcessCommandLine();
  cmdline->AppendSwitchPath(variations::switches::kVariationsStateFile,
                            temp_file);
  BASE_EXPECT_DEATH(MaybeUnpackVariationsStateFile(), "");
}

TEST(VariationsCommandLineTest,
     MaybeUnpackVariationsStateFile_NonInJsonFormat) {
  base::FilePath fp = base::CreateUniqueTempDirectoryScopedToTest();
  base::FilePath temp_file;
  CreateAndOpenTemporaryFileInDir(fp, &temp_file);
  CHECK(base::WriteFile(temp_file, base::Base64Encode("invalid json string")));
  base::test::ScopedCommandLine scoped_cmdline;
  base::CommandLine* cmdline = scoped_cmdline.GetProcessCommandLine();
  cmdline->AppendSwitchPath(variations::switches::kVariationsStateFile,
                            temp_file);
  BASE_EXPECT_DEATH(MaybeUnpackVariationsStateFile(), "");
}

}  // namespace variations
