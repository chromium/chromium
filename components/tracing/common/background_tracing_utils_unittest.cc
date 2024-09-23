// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_utils.h"

#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_proto_loader.h"
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

const char kValidProtoTracingConfig[] = R"pb(
  scenarios: {
    scenario_name: "test_scenario"
    start_rules: { name: "start_trigger" manual_trigger_name: "start_trigger" }
    upload_rules: {
      name: "upload_trigger"
      manual_trigger_name: "upload_trigger"
    }
    trace_config: {
      data_sources: { config: { name: "org.chromium.trace_metadata" } }
    }
  }
)pb";

const char kValidProtoRuleConfig[] = R"pb(
  rules: { name: "trigger1" manual_trigger_name: "trigger1" }
  rules: { name: "trigger2" manual_trigger_name: "trigger2" }
)pb";

std::string GetFieldTracingConfigFromText(const std::string& proto_text) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.ChromeFieldTracingConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  return serialized_message;
}

std::string GetTracingRulesConfigFromText(const std::string& proto_text) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.TracingTriggerRulesConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  return serialized_message;
}

struct SetupModeParams {
  const char* enable_background_tracing = nullptr;
  const char* enable_legacy_background_tracing = nullptr;
  BackgroundTracingSetupMode expected_mode;
};

TEST(BackgroundTracingUtilsTest, SetupFieldTracingFromFieldTrial) {
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  std::string serialized_config =
      GetFieldTracingConfigFromText(kValidProtoTracingConfig);
  std::string encoded_config = base::Base64Encode(serialized_config);
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(tracing::kFieldTracing,
                                                 {{"config", encoded_config}});

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_FALSE(tracing::SetupSystemTracingFromFieldTrial());
  EXPECT_TRUE(tracing::SetupFieldTracingFromFieldTrial());
}

TEST(BackgroundTracingUtilsTest, SetupSystemTracingFromFieldTrial) {
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  std::string serialized_config =
      GetTracingRulesConfigFromText(kValidProtoRuleConfig);
  std::string encoded_config = base::Base64Encode(serialized_config);
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(tracing::kTracingTriggers,
                                                 {{"config", encoded_config}});

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_TRUE(tracing::SetupSystemTracingFromFieldTrial());
}

TEST(BackgroundTracingUtilsTest, SetupBackgroundTracingFromProtoConfigFile) {
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("config.pb");
  base::WriteFile(file_path,
                  GetFieldTracingConfigFromText(kValidProtoTracingConfig));

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kBackgroundTracingOutputPath,
                                 temp_dir.GetPath());
  command_line->AppendSwitchPath(switches::kEnableBackgroundTracing, file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromProtoConfigFile);
  EXPECT_FALSE(tracing::SetupSystemTracingFromFieldTrial());
  EXPECT_FALSE(tracing::SetupFieldTracingFromFieldTrial());
  EXPECT_TRUE(tracing::SetupBackgroundTracingFromCommandLine());
}

TEST(BackgroundTracingUtilsTest, SetupFieldTracingFromFieldTrialOutputPath) {
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  std::string serialized_config =
      GetFieldTracingConfigFromText(kValidProtoTracingConfig);
  std::string encoded_config = base::Base64Encode(serialized_config);
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(tracing::kFieldTracing,
                                                 {{"config", encoded_config}});

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kBackgroundTracingOutputPath,
                                 temp_dir.GetPath());

  ASSERT_TRUE(tracing::HasBackgroundTracingOutputPath());
  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_TRUE(tracing::SetupFieldTracingFromFieldTrial());
}

TEST(BackgroundTracingUtilsTest, GetBackgroundTracingSetupMode) {
  base::test::TaskEnvironment task_env;
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();
  const std::vector<SetupModeParams> kParams = {
      // No config file param.
      {nullptr, nullptr, BackgroundTracingSetupMode::kFromFieldTrial},
      // Empty config filename.
      {"", nullptr, BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // Conflicting params.
      {"config.pb", "config.json",
       BackgroundTracingSetupMode::kDisabledInvalidCommandLine},
      // file is valid for proto traces.
      {"config.pb", nullptr, BackgroundTracingSetupMode::kFromProtoConfigFile},
      // file is valid for proto traces.
      {nullptr, "config.json", BackgroundTracingSetupMode::kFromJsonConfigFile},
  };

  for (const SetupModeParams& params : kParams) {
    SCOPED_TRACE(::testing::Message()
                 << "enable_background_tracing "
                 << params.enable_background_tracing
                 << "enable_legacy_background_tracing "
                 << params.enable_legacy_background_tracing);
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

    EXPECT_EQ(tracing::GetBackgroundTracingSetupMode(), params.expected_mode);
  }
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromJsonConfigFileFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableLegacyBackgroundTracing, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(
      tracing::SetupBackgroundTracingFromJsonConfigFile(base::FilePath()));
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromProtoConfigFileFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableBackgroundTracing, "");

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kDisabledInvalidCommandLine);
  EXPECT_FALSE(
      tracing::SetupBackgroundTracingFromProtoConfigFile(base::FilePath()));
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

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchPath(switches::kEnableLegacyBackgroundTracing,
                                 config_file_path);

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromJsonConfigFile);

  EXPECT_FALSE(
      tracing::SetupBackgroundTracingFromJsonConfigFile(config_file_path));
}

TEST_F(BackgroundTracingUtilTest, SetupBackgroundTracingWithOutputPathFailed) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputPath, "");

  EXPECT_TRUE(tracing::HasBackgroundTracingOutputPath());
  EXPECT_FALSE(tracing::SetBackgroundTracingOutputPath());
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromProtoConfigFileInvalidConfig) {
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
            BackgroundTracingSetupMode::kFromProtoConfigFile);

  EXPECT_FALSE(
      tracing::SetupBackgroundTracingFromProtoConfigFile(config_file_path));
}

TEST_F(BackgroundTracingUtilTest, SetupBackgroundTracingFromCommandLineConfig) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableLegacyBackgroundTracing,
                                  "config.json");
  command_line->AppendSwitchASCII(switches::kBackgroundTracingOutputPath,
                                  "test_trace.perfetto.gz");

  EXPECT_TRUE(tracing::HasBackgroundTracingOutputPath());
  EXPECT_TRUE(tracing::SetBackgroundTracingOutputPath());
  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromJsonConfigFile);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingUtilTest,
       SetupBackgroundTracingFromCommandLineFieldTrial) {
  auto background_tracing_manager =
      content::BackgroundTracingManager::CreateInstance();

  ASSERT_EQ(tracing::GetBackgroundTracingSetupMode(),
            BackgroundTracingSetupMode::kFromFieldTrial);
  EXPECT_FALSE(tracing::SetupBackgroundTracingFromCommandLine());
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

}  // namespace
