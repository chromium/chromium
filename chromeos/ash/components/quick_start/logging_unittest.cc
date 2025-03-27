// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chromeos/ash/components/quick_start/logging.h"

namespace ash::quick_start {

namespace {

const char kLog1[] = "Mahogony destined to make a sturdy table";
const char kLog2[] = "Construction grade cedar";
const char kLog3[] = "Pine infested by hungry beetles";
const char kLog4[] = "Unremarkable maple";

// Called for every log message added to the standard logging system. The new
// log is saved in |standard_logs| and consumed so it does not flood stdout.
std::vector<std::string>& GetStandardLogs() {
  static base::NoDestructor<std::vector<std::string>> standard_logs;
  return *standard_logs;
}

bool HandleStandardLogMessage(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& str) {
  GetStandardLogs().push_back(str);
  return true;
}

}  // namespace

class QuickStartLoggingTest : public testing::Test {
 public:
  QuickStartLoggingTest() = default;

  void SetUp() override {
    GetStandardLogs().clear();

    previous_handler_ = logging::GetLogMessageHandler();
    logging::SetLogMessageHandler(&HandleStandardLogMessage);
  }

  void TearDown() override { logging::SetLogMessageHandler(previous_handler_); }

 private:
  logging::LogMessageHandlerFunction previous_handler_{nullptr};
};

TEST_F(QuickStartLoggingTest, NonVerboseStandardLogsCreated) {
  QS_LOG(INFO) << kLog1;
  QS_LOG(WARNING) << kLog2;
  QS_LOG(ERROR) << kLog3;
  QS_LOG(VERBOSE) << kLog4;

  ASSERT_EQ(3u, GetStandardLogs().size());
  EXPECT_NE(std::string::npos, GetStandardLogs()[0].find(kLog1));
  EXPECT_NE(std::string::npos, GetStandardLogs()[1].find(kLog2));
  EXPECT_NE(std::string::npos, GetStandardLogs()[2].find(kLog3));
}

TEST_F(QuickStartLoggingTest, VerboseStandardLogsCreatedWithFlagEnabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--quick-start-verbose-logging"});

  QS_LOG(INFO) << kLog1;
  QS_LOG(WARNING) << kLog2;
  QS_LOG(ERROR) << kLog3;
  QS_LOG(VERBOSE) << kLog4;

  ASSERT_EQ(4u, GetStandardLogs().size());
  EXPECT_NE(std::string::npos, GetStandardLogs()[0].find(kLog1));
  EXPECT_NE(std::string::npos, GetStandardLogs()[1].find(kLog2));
  EXPECT_NE(std::string::npos, GetStandardLogs()[2].find(kLog3));
  EXPECT_NE(std::string::npos, GetStandardLogs()[3].find(kLog4));
}

}  // namespace ash::quick_start
