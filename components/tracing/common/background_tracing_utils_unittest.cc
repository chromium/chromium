// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using tracing::BackgroundTracingSetupMode;

namespace {

class BackgroundTracingUtilTest : public testing::Test {
  base::test::TaskEnvironment task_env;
};

const char kInvalidTracingConfig[] = "{][}";

struct SetupModeParams {
  const char* enable_background_tracing = nullptr;
  const char* enable_legacy_background_tracing = nullptr;
  const char* trace_output_file = nullptr;
  BackgroundTracingSetupMode expected_mode;
};

TEST(BackgroundTracingUtilsTest, GetBackgroundTracingSetupMode) {
  base::test::TaskEnvironment task_env;
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();
  const std::vector<SetupModeParams> kParams = {
      // No config file param.
      {nullptr, nullptr, nullptr, BackgroundTracingSetupMode::kFromFieldTrial},
      // Empty config filename.
      {"", nullptr, "output_file.gz",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // No output location switch.
      {"config.pb", nullptr, nullptr,
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // Empty output location switch.
      {"config.pb", nullptr, "",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // Conflicting params.
      {"config.pb", "config.json", nullptr,
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // file is valid for proto traces.
      {"config.pb", nullptr, "output_file.gz",
       BackgroundTracingSetupMode::kFromProtoConfigFile},
      // file is valid for proto traces.
      {nullptr, "config.json", "output_file.gz",
       BackgroundTracingSetupMode::kFromJsonConfigFile},
      // Field trial with output location switch.
      {nullptr, nullptr, "output_file.gz",
       BackgroundTracingSetupMode::kFromFieldTrialLocalOutput},
      // Field trial, empty output location switch.
      {nullptr, nullptr, "",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
  };

  for (const SetupModeParams& params : kParams) {
    SCOPED_TRACE(::testing::Message()
                 << "enable_background_tracing "
                 << params.enable_background_tracing
                 << "enable_legacy_background_tracing "
                 << params.enable_legacy_background_tracing
                 << " trace_output_file " << params.trace_output_file);
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    if (params.enable_background_tracing) {
      command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing,
                                      params.enable_background_tracing);
    }
    if (params.enable_legacy_background_tracing) {
      command_line->AppendSwitchASCII(switches::kEnableLegacyBackgroundTracing,
                                      params.enable_legacy_background_tracing);
    }
    if (params.trace_output_file) {
      command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile,
                                      params.trace_output_file);
    }

    EXPECT_EQ(tracing::GetBackgroundTracingSetupMode(), params.expected_mode);
  }
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromJsonConfigFileFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");
  command_line->AppendSwitchASCII(switches::kEnableLegacyBackgroundTracing, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromJsonConfigFile(
      base::FilePath(), base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromProtoConfigFileFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");
  command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromProtoConfigFile(
      base::FilePath(), base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromJsonConfigFileEmptyOutputFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path =
      temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(config_file_path, kInvalidTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableLegacyBackgroundTracing,
                                 config_file_path);
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromJsonConfigFile(
      config_file_path, base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromProtoConfigFileEmptyOutputFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path = temp_dir.GetPath().AppendASCII("config.pb");
  base::WriteFile(config_file_path, kInvalidTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing,
                                 config_file_path);
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromProtoConfigFile(
      config_file_path, base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromJsonConfigFileMissingOutputFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path =
      temp_dir.GetPath().AppendASCII("config.json");
  base::WriteFile(config_file_path, kInvalidTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableLegacyBackgroundTracing,
                                 config_file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromJsonConfigFile(
      config_file_path, base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromProtoConfigFileMissingOutputFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path = temp_dir.GetPath().AppendASCII("config.pb");
  base::WriteFile(config_file_path, kInvalidTracingConfig);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing,
                                 config_file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromProtoConfigFile(
      config_file_path, base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromJsonConfigFileInvalidConfig) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

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
  command_line->AppendSwitchPath(switches::kEnableLegacyBackgroundTracing,
                                 config_file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromJsonConfigFile);

  EXPECT_FALSE(tracing::SetupBackgroundTracingFromJsonConfigFile(
      config_file_path, output_file_path));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromProtoConfigFileInvalidConfig) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath config_file_path = temp_dir.GetPath().AppendASCII("config.pb");
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
            BackgroundTracingSetupMode::kFromProtoConfigFile);

  EXPECT_FALSE(tracing::SetupBackgroundTracingFromProtoConfigFile(
      config_file_path, output_file_path));
}

TEST_F(BackgroundTracingUtilTest, SetupBackgroundTracingWithOutputFileFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(
      tracing::SetupBackgroundTracingWithOutputFile(nullptr, base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromCommandLineInvalid) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine(""));
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingUtilTest, SetupBackgroundTracingFromCommandLineConfig) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableLegacyBackgroundTracing,
                                  "config.json");
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputFile,
                                  "test_trace.perfetto.gz");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromJsonConfigFile);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine(""));
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromCommandLineFieldTrial) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine(""));
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

}  // namespace
