// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_command_line.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using tracing::BackgroundTracingSetupMode;

namespace {

const char kInvalidTracingConfig[] = "{][}";

struct SetupModeParams {
  const char* enable_background_tracing = nullptr;
  const char* trace_output_file = nullptr;
  BackgroundTracingSetupMode expected_mode;
};

TEST(BackgroundTracingUtilsTest, GetBackgroundTracingSetupMode) {
  const std::vector<SetupModeParams> kParams = {
      // No config file param.
      {nullptr, nullptr, BackgroundTracingSetupMode::kFromFieldTrial},
      // Empty config filename.
      {"", "output_file.gz",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // No output location switch.
      {"config.json", nullptr,
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // Empty output location switch.
      {"config.json", "",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // file is valid for proto traces.
      {"config.json", "output_file.gz",
       BackgroundTracingSetupMode::kFromConfigFile},
      // Field trial with output location switch.
      {nullptr, "output_file.gz",
       BackgroundTracingSetupMode::kFromFieldTrialLocalOutput},
      // Field trial, empty output location switch.
      {nullptr, "", BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
  };

  for (const SetupModeParams& params : kParams) {
    SCOPED_TRACE(::testing::Message()
                 << "enable_background_tracing "
                 << params.enable_background_tracing << " trace_output_file "
                 << params.trace_output_file);
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    if (params.enable_background_tracing) {
      command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing,
                                      params.enable_background_tracing);
    }
    if (params.trace_output_file) {
      command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile,
                                      params.trace_output_file);
    }

    EXPECT_EQ(tracing::GetBackgroundTracingSetupMode(), params.expected_mode);
  }
}

TEST(BackgroundTracingUtilTest, SetupBackgroundTracingFromConfigFileFailed) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");
  command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  tracing::SetupBackgroundTracingFromConfigFile(base::FilePath(),
                                                base::FilePath());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest,
     SetupBackgroundTracingFromConfigFileEmptyOutputFailed) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path =
      temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(config_file_path, kInvalidTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing,
                                 config_file_path);
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  tracing::SetupBackgroundTracingFromConfigFile(config_file_path,
                                                base::FilePath());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest,
     SetupBackgroundTracingFromConfigFileMissingOutputFailed) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path =
      temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(config_file_path, kInvalidTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing,
                                 config_file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  tracing::SetupBackgroundTracingFromConfigFile(config_file_path,
                                                base::FilePath());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest,
     SetupBackgroundTracingFromConfigFileInvalidConfig) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path =
      temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(config_file_path, kInvalidTracingConfig);
  auto output_file_path =
      temp_dir.GetPath().AppendASCII("test_trace.perfetto.gz");

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kBackgroundTracingOutputFile,
                                 output_file_path);
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing,
                                 config_file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromConfigFile);

  tracing::SetupBackgroundTracingFromConfigFile(config_file_path,
                                                output_file_path);
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest, SetupBackgroundTracingWithOutputFileFailed) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  tracing::SetupBackgroundTracingWithOutputFile(nullptr, base::FilePath());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest, SetupBackgroundTracingFromCommandLineInvalid) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine(""));
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest, SetupBackgroundTracingFromCommandLineConfig) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing,
                                  "config.json");
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile,
                                  "test_trace.perfetto.gz");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromConfigFile);
  EXPECT_TRUE(tracing::SetupBackgroundTracingFromCommandLine(""));
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST(BackgroundTracingUtilTest,
     SetupBackgroundTracingFromCommandLineFieldTrial) {
  ASSERT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine(""));
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

}  // namespace
