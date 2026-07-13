// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_logger.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class LoggerTestObserver : public OptimizationGuideLogger::Observer {
 public:
  LoggerTestObserver() {
    messages_.reserve(OptimizationGuideLogger::kMaxRecentLogMessages + 10);
  }
  ~LoggerTestObserver() override = default;

  void OnLogMessageAdded(base::Time event_time,
                         optimization_guide_common::mojom::LogSource log_source,
                         const std::string& source_file,
                         int source_line,
                         const std::string& message) override {
    messages_.push_back(message);
  }

  const std::vector<std::string>& messages() const { return messages_; }

 private:
  std::vector<std::string> messages_;
};

}  // namespace

TEST(OptimizationGuideLoggerTest, BufferOverflowGeneratesWarningOnObserverAdd) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDebugLoggingEnabled);

  OptimizationGuideLogger logger;
  EXPECT_TRUE(logger.ShouldEnableDebugLogs());

  const size_t max_messages = OptimizationGuideLogger::kMaxRecentLogMessages;
  // Emit max_messages + 5 messages to overflow the limit by 5.
  for (size_t i = 0; i < max_messages + 5; ++i) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
        &logger)
        << "Message " << base::NumberToString(i);
  }

  LoggerTestObserver observer;
  logger.AddObserver(&observer);

  // Should receive 1 warning message + max_messages preserved messages.
  ASSERT_EQ(max_messages + 1, observer.messages().size());
  EXPECT_THAT(
      observer.messages()[0],
      testing::HasSubstr(
          "⚠️ [WARNING]: 5 earlier startup debug log message(s) were dropped"));
  EXPECT_EQ("Message 5", observer.messages()[1]);
  EXPECT_EQ(base::StrCat({"Message ", base::NumberToString(max_messages + 4)}),
            observer.messages()[max_messages]);

  logger.RemoveObserver(&observer);
}

}  // namespace optimization_guide
