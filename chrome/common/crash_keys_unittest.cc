// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

using crash_reporter::GetCrashKeyValue;
using ::testing::IsEmpty;

class CrashKeysTest : public testing::Test {
 public:
  void SetUp() override { crash_reporter::InitializeCrashKeys(); }

  void TearDown() override {
    // Breakpad doesn't properly support ResetCrashKeysForTesting() and usually
    // CHECK fails after it is called.
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
    crash_reporter::ResetCrashKeysForTesting();
#endif
  }
};

TEST_F(CrashKeysTest, ShouldIgnoreBoringFlags) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::string expected_num_switches = "7";
  command_line.AppendSwitch("--enable-logging");
  command_line.AppendSwitch("--v=1");

  command_line.AppendSwitch("--vv=1");
  command_line.AppendSwitch("--vvv");
  command_line.AppendSwitch("--enable-multi-profiles");
  command_line.AppendSwitch("--device-management-url=https://foo/bar");
  command_line.AppendSwitch(
      base::StrCat({"--", switches::kGpuPreferences, "=ABC123"}));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_line.AppendSwitch("--user-data-dir=/tmp");
  command_line.AppendSwitch("--default-wallpaper-small=test.png");
  expected_num_switches = "9";
#endif

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("--vv=1", GetCrashKeyValue("switch-1"));
  EXPECT_EQ("--vvv", GetCrashKeyValue("switch-2"));
  EXPECT_EQ("--enable-multi-profiles", GetCrashKeyValue("switch-3"));
  EXPECT_EQ("--device-management-url=https://foo/bar",
            GetCrashKeyValue("switch-4"));
  EXPECT_EQ(expected_num_switches, GetCrashKeyValue("num-switches"));
  EXPECT_TRUE(GetCrashKeyValue("switch-5").empty());
}

namespace {

void InitFromArgv(base::CommandLine& command_line,
                  std::initializer_list<std::string> args) {
#if BUILDFLAG(IS_WIN)
  // Convert arguments to wstring on windows.
  base::CommandLine::StringVector arg_vector;
  for (std::string arg : args) {
    arg_vector.push_back(base::ASCIIToWide(arg));
  }
  command_line.InitFromArgv(arg_vector);
#else
  command_line.InitFromArgv(args);
#endif
}

}  // namespace

TEST_F(CrashKeysTest, AllocateCrashKeyInBrowserAndChildren) {
  crash_keys::AllocateCrashKeyInBrowserAndChildren("annotation-name",
                                                   "annotation-value");
  crash_keys::AllocateCrashKeyInBrowserAndChildren("another-name",
                                                   "another-value");

  EXPECT_EQ("annotation-value", GetCrashKeyValue("annotation-name"));
  EXPECT_EQ("another-value", GetCrashKeyValue("another-name"));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(command_line, {"program_name", "--type=renderer"});

  crash_keys::AppendStringAnnotationsCommandLineSwitch(&command_line);
  EXPECT_EQ("annotation-name=annotation-value,another-name=another-value",
            command_line.GetSwitchValueASCII("string-annotations"));

}

TEST_F(CrashKeysTest, SetStringAnnotationsBrowser) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(command_line, {"program_name",
                              "--string-annotations=annotation-name=annotation-"
                              "value,another-annotation=another-value"});

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("", GetCrashKeyValue("annotation-name"));
  EXPECT_EQ("", GetCrashKeyValue("another-annotation"));
}

TEST_F(CrashKeysTest, SetStringAnnotationsNonBrowser) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(command_line, {"program_name", "--type=renderer",
                              "--string-annotations=annotation-name=annotation-"
                              "value,another-annotation=another-value"});

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("annotation-value", GetCrashKeyValue("annotation-name"));
  EXPECT_EQ("another-value", GetCrashKeyValue("another-annotation"));
}

TEST_F(CrashKeysTest, EnabledDisabledFeaturesFlags) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(
      command_line,
      {"program_name", "--example", "--enable-features=AEnabledFeatureFlag",
       "--unrelated-flag=23", "--disable-features=ADisabledFeaturesString",
       "--more-example=yes"});

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("AEnabledFeatureFlag",
            GetCrashKeyValue("commandline-enabled-feature-1"));
  EXPECT_EQ("ADisabledFeaturesString",
            GetCrashKeyValue("commandline-disabled-feature-1"));

  // Unrelated flags are not affected by the enable-features extraction.
  EXPECT_EQ("--example", GetCrashKeyValue("switch-1"));
  EXPECT_EQ("--unrelated-flag=23", GetCrashKeyValue("switch-2"));
  EXPECT_EQ("--more-example=yes", GetCrashKeyValue("switch-3"));
  // --enable-features and --disable-features are still counted in num-switches.
  EXPECT_EQ("5", GetCrashKeyValue("num-switches"));
}

TEST_F(CrashKeysTest, ShouldCreateCrashKeyForEachEnabledFeature) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(command_line, {"program_name", "--example",
                              "--enable-features=FirstFeature,SecondFeature",
                              "--unrelated-flag=23", "--more-example=yes"});

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("FirstFeature", GetCrashKeyValue("commandline-enabled-feature-1"));
  EXPECT_EQ("SecondFeature", GetCrashKeyValue("commandline-enabled-feature-2"));

  EXPECT_EQ(GetCrashKeyValue("commandline-enabled-feature-3"), "");
}

TEST_F(CrashKeysTest, ShouldCreateCrashKeyForEachDisabledFeature) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(command_line, {"program_name", "--example",
                              "--disable-features=FirstFeature,SecondFeature",
                              "--unrelated-flag=23", "--more-example=yes"});

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("FirstFeature", GetCrashKeyValue("commandline-disabled-feature-1"));
  EXPECT_EQ("SecondFeature",
            GetCrashKeyValue("commandline-disabled-feature-2"));

  EXPECT_EQ(GetCrashKeyValue("commandline-disabled-feature-3"), "");
}

TEST_F(CrashKeysTest,
       EnabledDisabledFeatures_LastCommandArgumentShouldBeRetained) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  InitFromArgv(
      command_line,
      {"program_name", "--enable-features=FeatureEnabledInFirstArgument",
       "--disable-features=FeatureDisabledInFirstArgument",
       "--enable-features=FeatureEnabledInSecondArgument",
       "--disable-features=FeatureDisabledInSecondArgument"});

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("FeatureEnabledInSecondArgument",
            GetCrashKeyValue("commandline-enabled-feature-1"));
  EXPECT_EQ("FeatureDisabledInSecondArgument",
            GetCrashKeyValue("commandline-disabled-feature-1"));
}

TEST_F(
    CrashKeysTest,
    EnabledDisabledFeatures_ShouldClearPreviousCrashKeysIfCommandLineIsReparsed) {
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    InitFromArgv(
        command_line,
        {"program_name", "--enable-features=OriginalEnable_1, OriginalEnable_2",
         "--disable-features=OriginalDisable_1, OriginalDisable_2"});
    crash_keys::SetCrashKeysFromCommandLine(command_line);

    EXPECT_EQ("OriginalEnable_1",
              GetCrashKeyValue("commandline-enabled-feature-1"));
    EXPECT_EQ("OriginalEnable_2",
              GetCrashKeyValue("commandline-enabled-feature-2"));

    EXPECT_EQ("OriginalDisable_1",
              GetCrashKeyValue("commandline-disabled-feature-1"));
    EXPECT_EQ("OriginalDisable_2",
              GetCrashKeyValue("commandline-disabled-feature-2"));
  }

  // Parse a command line with only a single value in each flag.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    InitFromArgv(command_line, {"program_name", "--enable-features=NewEnable",
                                "--disable-features=NewDisable"});
    crash_keys::SetCrashKeysFromCommandLine(command_line);

    EXPECT_EQ("NewEnable", GetCrashKeyValue("commandline-enabled-feature-1"));
    EXPECT_EQ(GetCrashKeyValue("commandline-enabled-feature-2"), "");

    EXPECT_EQ("NewDisable", GetCrashKeyValue("commandline-disabled-feature-1"));
    EXPECT_EQ(GetCrashKeyValue("commandline-disabled-feature-2"), "");
  }

  // Parse a command line with no enable-features or disable-features flags.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    InitFromArgv(command_line,
                 {"program_name", "--enable-logging", "--type=renderer"});
    crash_keys::SetCrashKeysFromCommandLine(command_line);
    EXPECT_EQ(GetCrashKeyValue("commandline-enabled-feature-1"), "");
    EXPECT_EQ(GetCrashKeyValue("commandline-enabled-feature-2"), "");
    EXPECT_EQ(GetCrashKeyValue("commandline-disabled-feature-1"), "");
    EXPECT_EQ(GetCrashKeyValue("commandline-disabled-feature-2"), "");
  }

  // Parse a new command line with enable-features or disable-features flags.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    InitFromArgv(command_line, {"program_name", "--enable-features=NewEnable",
                                "--disable-features=NewDisable"});
    crash_keys::SetCrashKeysFromCommandLine(command_line);
    EXPECT_EQ("NewEnable", GetCrashKeyValue("commandline-enabled-feature-1"));
    EXPECT_EQ("NewDisable", GetCrashKeyValue("commandline-disabled-feature-1"));
  }
}
