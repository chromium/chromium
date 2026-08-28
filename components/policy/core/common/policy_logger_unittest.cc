// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace policy {

namespace {

void AddLogs(const std::string& message, PolicyLogger* policy_logger) {
  LOG_POLICY(INFO, POLICY_FETCHING) << "Element added: " << message;
}

size_t GetLogCount(PolicyLogger* logger) {
  return logger->GetAsList().size();
}

}  // namespace

class PolicyLoggerTest : public PlatformTest {
 public:
  PolicyLoggerTest() = default;
  ~PolicyLoggerTest() override = default;

 protected:
  // Clears the logs list before the test. This is important to prevent tests
  // from affecting each other's results.
  void SetUp() override {
    policy::PolicyLogger::GetInstance()->ResetLoggerForTesting();
  }
};

// Checks that the logger is enabled by feature and that `GetAsList` returns an
// updated list of logs.
TEST_F(PolicyLoggerTest, PolicyLoggingEnabled) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  size_t log_count_before_adding = GetLogCount(policy_logger);
  AddLogs("when the feature is enabled.", policy_logger);

  EXPECT_EQ(GetLogCount(policy_logger), log_count_before_adding + 1);
  EXPECT_EQ(*(policy_logger->GetAsList()[log_count_before_adding]
                  .GetDict()
                  .FindString("message")),
            "Element added: when the feature is enabled.");
}

// Checks that the first log added is deleted when `PolicyLogger::kMaxLogCount`
// is exceeded.
TEST_F(PolicyLoggerTest, MaxCountExceededDeletesOldestLog) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  AddLogs("First log that will be removed.", policy_logger);

  // Adds `kMaxLogCount` - 1 more elements until `kMaxLogCount` is reached.
  for (size_t i = 0; i < policy::PolicyLogger::kMaxLogCount - 1; i++) {
    AddLogs(base::NumberToString(i + 1), policy_logger);
  }
  EXPECT_EQ(GetLogCount(policy_logger), policy::PolicyLogger::kMaxLogCount);

  AddLogs("Last log added and size is exceeded.", policy_logger);

  size_t current_count = GetLogCount(policy_logger);
  base::ListValue current_logs = policy_logger->GetAsList();

  EXPECT_EQ(current_count, policy::PolicyLogger::kMaxLogCount);

  EXPECT_EQ(*(current_logs[0].GetDict().FindString("message")),
            "Element added: 1");

  EXPECT_EQ(*(current_logs[current_count - 1].GetDict().FindString("message")),
            "Element added: Last log added and size is exceeded.");
}

}  // namespace policy
