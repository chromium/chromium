// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/trace_startup_config.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/tracing/common/tracing_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

namespace {

const char kTraceConfig[] =
    "{"
    "\"enable_argument_filter\":true,"
    "\"enable_systrace\":true,"
    "\"excluded_categories\":[\"excluded\",\"exc_pattern*\"],"
    "\"included_categories\":[\"included\","
    "\"inc_pattern*\","
    "\"disabled-by-default-cc\"],"
    "\"record_mode\":\"record-continuously\""
    "}";

std::string GetTraceConfigFileContent(std::string trace_config,
                                      std::string startup_duration,
                                      std::string result_file) {
  std::string content = "{";
  if (!trace_config.empty())
    content += "\"trace_config\":" + trace_config;

  if (!startup_duration.empty()) {
    if (content != "{")
      content += ",";
    content += "\"startup_duration\":" + startup_duration;
  }

  if (!result_file.empty()) {
    if (content != "{")
      content += ",";
    content += "\"result_file\":\"" + result_file + "\"";
  }

  content += "}";
  return content;
}

}  // namespace

TEST(TraceStartupConfigTest, TraceStartupEnabled) {
  base::ShadowingAtExitManager sem;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTraceStartup);

  EXPECT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, TraceStartupConfigNotEnabled) {
  base::ShadowingAtExitManager sem;
  EXPECT_FALSE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, TraceStartupConfigEnabledWithoutPath) {
  base::ShadowingAtExitManager sem;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kTraceConfigFile);

  ASSERT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_EQ(base::trace_event::TraceConfig().ToString(),
            TraceStartupConfig::GetInstance()->GetTraceConfig().ToString());
  EXPECT_EQ(5, TraceStartupConfig::GetInstance()->GetStartupDuration());
  EXPECT_TRUE(TraceStartupConfig::GetInstance()->GetResultFile().empty());
}

TEST(TraceStartupConfigTest, TraceStartupConfigEnabledWithInvalidPath) {
  base::ShadowingAtExitManager sem;
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile,
      base::FilePath(FILE_PATH_LITERAL("invalid-trace-config-file-path")));

  EXPECT_FALSE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, ValidContent) {
  base::ShadowingAtExitManager sem;
  std::string content =
      GetTraceConfigFileContent(kTraceConfig, "10", "trace_result_file.log");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_NE(-1, base::WriteFile(trace_config_file, content.c_str(),
                                (int)content.length()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  ASSERT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_STREQ(
      kTraceConfig,
      TraceStartupConfig::GetInstance()->GetTraceConfig().ToString().c_str());
  EXPECT_EQ(10, TraceStartupConfig::GetInstance()->GetStartupDuration());
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("trace_result_file.log")),
            TraceStartupConfig::GetInstance()->GetResultFile());
}

TEST(TraceStartupConfigTest, ValidContentWithOnlyTraceConfig) {
  base::ShadowingAtExitManager sem;
  std::string content = GetTraceConfigFileContent(kTraceConfig, "", "");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_NE(-1, base::WriteFile(trace_config_file, content.c_str(),
                                (int)content.length()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  ASSERT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_STREQ(
      kTraceConfig,
      TraceStartupConfig::GetInstance()->GetTraceConfig().ToString().c_str());
  EXPECT_EQ(0, TraceStartupConfig::GetInstance()->GetStartupDuration());
  EXPECT_TRUE(TraceStartupConfig::GetInstance()->GetResultFile().empty());
}

TEST(TraceStartupConfigTest, ContentWithAbsoluteResultFilePath) {
  base::ShadowingAtExitManager sem;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath result_file_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("trace_result_file.log"));
  ASSERT_TRUE(result_file_path.IsAbsolute());

  std::string result_file_path_str = result_file_path.AsUTF8Unsafe();
  auto it =
      std::find(result_file_path_str.begin(), result_file_path_str.end(), '\\');
  while (it != result_file_path_str.end()) {
    auto it2 = result_file_path_str.insert(it, '\\');
    it = std::find(it2 + 2, result_file_path_str.end(), '\\');
  }
  std::string content =
      GetTraceConfigFileContent(kTraceConfig, "10", result_file_path_str);

  base::FilePath trace_config_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_NE(-1, base::WriteFile(trace_config_file, content.c_str(),
                                (int)content.length()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  ASSERT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_EQ(result_file_path,
            TraceStartupConfig::GetInstance()->GetResultFile());
}

TEST(TraceStartupConfigTest, ContentWithNegtiveDuration) {
  base::ShadowingAtExitManager sem;
  std::string content = GetTraceConfigFileContent(kTraceConfig, "-1", "");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_NE(-1, base::WriteFile(trace_config_file, content.c_str(),
                                (int)content.length()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  ASSERT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_STREQ(
      kTraceConfig,
      TraceStartupConfig::GetInstance()->GetTraceConfig().ToString().c_str());
  EXPECT_EQ(0, TraceStartupConfig::GetInstance()->GetStartupDuration());
  EXPECT_TRUE(TraceStartupConfig::GetInstance()->GetResultFile().empty());
}

TEST(TraceStartupConfigTest, ContentWithoutTraceConfig) {
  base::ShadowingAtExitManager sem;
  std::string content =
      GetTraceConfigFileContent("", "10", "trace_result_file.log");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_NE(-1, base::WriteFile(trace_config_file, content.c_str(),
                                (int)content.length()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  EXPECT_FALSE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, InvalidContent) {
  base::ShadowingAtExitManager sem;
  std::string content = "invalid trace config file content";

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_NE(-1, base::WriteFile(trace_config_file, content.c_str(),
                                (int)content.length()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  EXPECT_FALSE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, EmptyContent) {
  base::ShadowingAtExitManager sem;
  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  EXPECT_FALSE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, TraceStartupDisabledSystemOwner) {
  base::ShadowingAtExitManager sem;
  // Set the owner to 'system' is not sufficient to setup startup tracing.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTraceStartupOwner, "system");
  EXPECT_FALSE(TraceStartupConfig::GetInstance()->IsEnabled());
}

TEST(TraceStartupConfigTest, TraceStartupEnabledSystemOwner) {
  base::ShadowingAtExitManager sem;
  // With owner and --trace-startup TraceStartupConfig should be enabled with
  // the owner being the system.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTraceStartupOwner, "system");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTraceStartup);
  EXPECT_TRUE(TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_EQ(TraceStartupConfig::SessionOwner::kSystemTracing,
            TraceStartupConfig::GetInstance()->GetSessionOwner());
}

}  // namespace tracing
