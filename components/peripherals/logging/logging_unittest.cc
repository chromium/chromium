// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/peripherals/logging/logging.h"

#include <stddef.h>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/peripherals/logging/log_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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

class PeripheralsLoggingTest : public testing::Test {
 public:
  PeripheralsLoggingTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnablePeripheralsLogging);

    PeripheralsLogBuffer::GetInstance()->Clear();
    GetStandardLogs().clear();

    previous_handler_ = logging::GetLogMessageHandler();
    logging::SetLogMessageHandler(&HandleStandardLogMessage);
  }

  void TearDown() override { logging::SetLogMessageHandler(previous_handler_); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  logging::LogMessageHandlerFunction previous_handler_{nullptr};
};

TEST_F(PeripheralsLoggingTest, LogsSavedToBuffer) {
  int base_line_number = __LINE__;
  PR_LOG(INFO, Feature::ACCEL) << kLog1;
  PR_LOG(WARNING, Feature::IDS) << kLog2;
  PR_LOG(ERROR, Feature::ACCEL) << kLog3;
  PR_LOG(VERBOSE, Feature::IDS) << kLog4;

  auto* logs = PeripheralsLogBuffer::GetInstance()->logs();
  ASSERT_EQ(4u, logs->size());

  auto iterator = logs->begin();
  const PeripheralsLogBuffer::LogMessage& log_message1 = *iterator;
  EXPECT_EQ(base::JoinString({"[ACCEL]", kLog1}, " "), log_message1.text);
  EXPECT_EQ(__FILE__, log_message1.file);
  EXPECT_EQ(Feature::ACCEL, log_message1.feature);
  EXPECT_EQ(base_line_number + 1, log_message1.line);
  EXPECT_EQ(logging::LOGGING_INFO, log_message1.severity);

  ++iterator;
  const PeripheralsLogBuffer::LogMessage& log_message2 = *iterator;
  EXPECT_EQ(base::JoinString({"[IDS]", kLog2}, " "), log_message2.text);
  EXPECT_EQ(__FILE__, log_message2.file);
  EXPECT_EQ(Feature::IDS, log_message2.feature);
  EXPECT_EQ(base_line_number + 2, log_message2.line);
  EXPECT_EQ(logging::LOGGING_WARNING, log_message2.severity);

  ++iterator;
  const PeripheralsLogBuffer::LogMessage& log_message3 = *iterator;
  EXPECT_EQ(base::JoinString({"[ACCEL]", kLog3}, " "), log_message3.text);
  EXPECT_EQ(__FILE__, log_message3.file);
  EXPECT_EQ(Feature::ACCEL, log_message3.feature);
  EXPECT_EQ(base_line_number + 3, log_message3.line);
  EXPECT_EQ(logging::LOGGING_ERROR, log_message3.severity);

  ++iterator;
  const PeripheralsLogBuffer::LogMessage& log_message4 = *iterator;
  EXPECT_EQ(base::JoinString({"[IDS]", kLog4}, " "), log_message4.text);
  EXPECT_EQ(__FILE__, log_message4.file);
  EXPECT_EQ(Feature::IDS, log_message4.feature);
  EXPECT_EQ(base_line_number + 4, log_message4.line);
  EXPECT_EQ(logging::LOGGING_VERBOSE, log_message4.severity);
}

TEST_F(PeripheralsLoggingTest, LogWhenBufferIsFull) {
  PeripheralsLogBuffer* log_buffer = PeripheralsLogBuffer::GetInstance();
  EXPECT_EQ(0u, log_buffer->logs()->size());

  for (size_t i = 0; i < log_buffer->MaxBufferSize(); ++i) {
    PR_LOG(INFO, Feature::IDS) << "log " << i;
  }

  EXPECT_EQ(log_buffer->MaxBufferSize(), log_buffer->logs()->size());
  PR_LOG(INFO, Feature::IDS) << kLog1;
  EXPECT_EQ(log_buffer->MaxBufferSize(), log_buffer->logs()->size());

  auto iterator = log_buffer->logs()->begin();
  for (size_t i = 0; i < log_buffer->MaxBufferSize() - 1; ++iterator, ++i) {
    std::string expected_text = "[IDS] log " + base::NumberToString(i + 1);
    EXPECT_EQ(expected_text, (*iterator).text);
  }
  EXPECT_EQ(base::JoinString({"[IDS]", kLog1}, " "), (*iterator).text);
}

TEST_F(PeripheralsLoggingTest, StandardLogsCreated) {
  PR_LOG(INFO, Feature::IDS) << kLog1;
  PR_LOG(WARNING, Feature::IDS) << kLog2;
  PR_LOG(ERROR, Feature::IDS) << kLog3;
  PR_LOG(VERBOSE, Feature::IDS) << kLog4;

  ASSERT_EQ(4u, GetStandardLogs().size());
  EXPECT_NE(std::string::npos, GetStandardLogs()[0].find(kLog1));
  EXPECT_NE(std::string::npos, GetStandardLogs()[1].find(kLog2));
  EXPECT_NE(std::string::npos, GetStandardLogs()[2].find(kLog3));
  EXPECT_NE(std::string::npos, GetStandardLogs()[3].find(kLog4));
}

}  // namespace ash
