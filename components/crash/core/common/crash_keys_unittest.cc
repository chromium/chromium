// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using crash_reporter::GetCrashKeyValue;

// The number of switch-N keys declared in SetSwitchesFromCommandLine().
constexpr int kSwitchesMaxCount = 15;

class CrashKeysTest : public testing::Test {
 public:
  void SetUp() override {
    ResetData();
    crash_reporter::InitializeCrashKeysForTesting();
  }

  void TearDown() override {
    ResetData();
  }

 private:
  void ResetData() {
    crash_keys::ResetCommandLineForTesting();
    crash_reporter::ResetCrashKeysForTesting();
  }
};

TEST_F(CrashKeysTest, Switches) {
  // Set three switches.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    for (size_t i = 1; i <= 3; ++i)
      command_line.AppendSwitch(base::StringPrintf("--flag-%" PRIuS, i));
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
    EXPECT_EQ("--flag-1", GetCrashKeyValue("switch-1"));
    EXPECT_EQ("--flag-2", GetCrashKeyValue("switch-2"));
    EXPECT_EQ("--flag-3", GetCrashKeyValue("switch-3"));
    EXPECT_TRUE(GetCrashKeyValue("switch-4").empty());
  }

  // Set more than 15 switches.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    const size_t kMax = kSwitchesMaxCount + 2;
    EXPECT_GT(kMax, static_cast<size_t>(15));
    for (size_t i = 1; i <= kMax; ++i)
      command_line.AppendSwitch(base::StringPrintf("--many-%" PRIuS, i));
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
    EXPECT_EQ("--many-1", GetCrashKeyValue("switch-1"));
    EXPECT_EQ("--many-9", GetCrashKeyValue("switch-9"));
    EXPECT_EQ("--many-15", GetCrashKeyValue("switch-15"));
    EXPECT_FALSE(GetCrashKeyValue("switch-16").empty());
    EXPECT_FALSE(GetCrashKeyValue("switch-17").empty());
  }

  // Set fewer to ensure that old ones are erased.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    for (int i = 1; i <= 5; ++i)
      command_line.AppendSwitch(base::StringPrintf("--fewer-%d", i));
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
    EXPECT_EQ("--fewer-1", GetCrashKeyValue("switch-1"));
    EXPECT_EQ("--fewer-2", GetCrashKeyValue("switch-2"));
    EXPECT_EQ("--fewer-3", GetCrashKeyValue("switch-3"));
    EXPECT_EQ("--fewer-4", GetCrashKeyValue("switch-4"));
    EXPECT_EQ("--fewer-5", GetCrashKeyValue("switch-5"));
    for (int i = 6; i < 20; ++i)
      EXPECT_TRUE(GetCrashKeyValue(base::StringPrintf("switch-%d", i)).empty());
  }
}

namespace {

bool IsBoringFlag(const std::string& flag) {
  return flag.compare("--boring") == 0;
}

}  // namespace

TEST_F(CrashKeysTest, FilterFlags) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("--not-boring-1");
  command_line.AppendSwitch("--boring");

  // Include the max number of non-boring switches, to make sure that only the
  // switches actually included in the crash keys are counted.
  for (size_t i = 2; i <= kSwitchesMaxCount; ++i)
    command_line.AppendSwitch(base::StringPrintf("--not-boring-%" PRIuS, i));

  crash_keys::SetSwitchesFromCommandLine(command_line, &IsBoringFlag);

  // If the boring keys are filtered out, every single key should now be
  // not-boring.
  for (int i = 1; i <= kSwitchesMaxCount; ++i) {
    std::string switch_name = base::StringPrintf("switch-%d", i);
    std::string switch_value = base::StringPrintf("--not-boring-%d", i);
    EXPECT_EQ(switch_value, GetCrashKeyValue(switch_name))
        << "switch_name is " << switch_name;
  }
}

TEST_F(CrashKeysTest, PrinterInfoReset) {
  // After ScopedPrinterInfo goes out of scope, printer keys should be reset.
  {
    const std::vector<std::string> kPrinterInfoFull{"1", "2", "3", "4"};
    crash_keys::ScopedPrinterInfo keys("dummy-printer", kPrinterInfoFull);
    EXPECT_EQ(GetCrashKeyValue("prn-info-1"), "1");
    EXPECT_EQ(GetCrashKeyValue("prn-info-2"), "2");
    EXPECT_EQ(GetCrashKeyValue("prn-info-3"), "3");
    EXPECT_EQ(GetCrashKeyValue("prn-info-4"), "4");
  }
  EXPECT_TRUE(GetCrashKeyValue("prn-info-1").empty());
  EXPECT_TRUE(GetCrashKeyValue("prn-info-2").empty());
  EXPECT_TRUE(GetCrashKeyValue("prn-info-3").empty());
  EXPECT_TRUE(GetCrashKeyValue("prn-info-4").empty());
}

TEST_F(CrashKeysTest, PrinterInfoContainsSemicolon) {
  // Provided data containing semicolons does not get split between keys.
  constexpr char kWithoutSemicolon[] = "printer name";
  constexpr char kWithSemicolon[] = "CUPS version 1.2; OS version 3.4";
  const std::vector<std::string> kPrinterInfo{kWithoutSemicolon,
                                              kWithSemicolon};
  crash_keys::ScopedPrinterInfo keys("dummy-printer", kPrinterInfo);
  EXPECT_EQ(GetCrashKeyValue("prn-info-1"), kWithoutSemicolon);
  EXPECT_EQ(GetCrashKeyValue("prn-info-2"), kWithSemicolon);
  EXPECT_TRUE(GetCrashKeyValue("prn-info-3").empty());
  EXPECT_TRUE(GetCrashKeyValue("prn-info-4").empty());
}

TEST_F(CrashKeysTest, PrinterInfoNoData) {
  // Provided data has no entries, so printer name is used for one key.
  constexpr char kPrinterName[] = "dummy-printer";
  crash_keys::ScopedPrinterInfo keys(kPrinterName, std::vector<std::string>());
  EXPECT_EQ(GetCrashKeyValue("prn-info-1"), kPrinterName);
  EXPECT_TRUE(GetCrashKeyValue("prn-info-2").empty());
  EXPECT_TRUE(GetCrashKeyValue("prn-info-3").empty());
  EXPECT_TRUE(GetCrashKeyValue("prn-info-4").empty());
}
