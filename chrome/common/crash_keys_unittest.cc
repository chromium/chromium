// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using crash_reporter::GetCrashKeyValue;

class CrashKeysTest : public testing::Test {
 public:
  void SetUp() override {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeys();
  }

  void TearDown() override {
    crash_reporter::ResetCrashKeysForTesting();
  }
};

TEST_F(CrashKeysTest, Extensions) {
  // Set three extensions.
  {
    std::set<std::string> extensions;
    extensions.insert("ext.1");
    extensions.insert("ext.2");
    extensions.insert("ext.3");

    crash_keys::SetActiveExtensions(extensions);

    extensions.erase(GetCrashKeyValue("extension-1"));
    extensions.erase(GetCrashKeyValue("extension-2"));
    extensions.erase(GetCrashKeyValue("extension-3"));
    EXPECT_EQ(0u, extensions.size());

    EXPECT_EQ("3", GetCrashKeyValue("num-extensions"));
    EXPECT_TRUE(GetCrashKeyValue("extension-4").empty());
  }

  // Set more than the max switches.
  {
    std::set<std::string> extensions;
    const int kMax = 12;
    for (int i = 1; i <= kMax; ++i)
      extensions.insert(base::StringPrintf("ext.%d", i));
    crash_keys::SetActiveExtensions(extensions);

    for (int i = 1; i <= kMax; ++i) {
      extensions.erase(GetCrashKeyValue(base::StringPrintf("extension-%d", i)));
    }
    EXPECT_EQ(2u, extensions.size());

    EXPECT_EQ("12", GetCrashKeyValue("num-extensions"));
    EXPECT_TRUE(GetCrashKeyValue("extension-13").empty());
    EXPECT_TRUE(GetCrashKeyValue("extension-14").empty());
  }

  // Set fewer to ensure that old ones are erased.
  {
    std::set<std::string> extensions;
    for (int i = 1; i <= 5; ++i)
      extensions.insert(base::StringPrintf("ext.%d", i));
    crash_keys::SetActiveExtensions(extensions);

    extensions.erase(GetCrashKeyValue("extension-1"));
    extensions.erase(GetCrashKeyValue("extension-2"));
    extensions.erase(GetCrashKeyValue("extension-3"));
    extensions.erase(GetCrashKeyValue("extension-4"));
    extensions.erase(GetCrashKeyValue("extension-5"));
    EXPECT_EQ(0u, extensions.size());

    EXPECT_EQ("5", GetCrashKeyValue("num-extensions"));
    for (int i = 6; i < 20; ++i) {
      std::string key = base::StringPrintf("extension-%d", i);
      EXPECT_TRUE(GetCrashKeyValue(key).empty()) << key;
    }
  }
}

TEST_F(CrashKeysTest, IgnoreBoringFlags) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("--enable-logging");
  command_line.AppendSwitch("--v=1");

  command_line.AppendSwitch("--vv=1");
  command_line.AppendSwitch("--vvv");
  command_line.AppendSwitch("--enable-multi-profiles");
  command_line.AppendSwitch("--device-management-url=https://foo/bar");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_line.AppendSwitch("--user-data-dir=/tmp");
  command_line.AppendSwitch("--default-wallpaper-small=test.png");
#endif

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("--vv=1", GetCrashKeyValue("switch-1"));
  EXPECT_EQ("--vvv", GetCrashKeyValue("switch-2"));
  EXPECT_EQ("--enable-multi-profiles", GetCrashKeyValue("switch-3"));
  EXPECT_EQ("--device-management-url=https://foo/bar",
            GetCrashKeyValue("switch-4"));
  EXPECT_TRUE(GetCrashKeyValue("switch-5").empty());
}
