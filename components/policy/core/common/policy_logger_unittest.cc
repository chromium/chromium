// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace policy {

namespace {

void AddLogs(const std::string& message, PolicyLogger* policy_logger) {
  LOG_POLICY(INFO, POLICY_FETCHING) << "Element added: " << message;
}

base::ListValue GetLogsAsList(PolicyLogger* policy_logger) {
  base::test::TestFuture<base::ListValue> future;
  policy_logger->GetAsList(future.GetCallback());
  return future.Take();
}

std::vector<policy::mojom::LogPtr> GetLogsAsMojoList(
    PolicyLogger* policy_logger) {
  base::test::TestFuture<std::vector<policy::mojom::LogPtr>> future;
  policy_logger->GetAsMojoList(future.GetCallback());
  return future.Take();
}

size_t GetLogCount(PolicyLogger* logger) {
  return GetLogsAsList(logger).size();
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

  base::test::TaskEnvironment task_environment_;
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

  base::ListValue logs = GetLogsAsList(policy_logger);
  EXPECT_EQ(logs.size(), log_count_before_adding + 1);
  EXPECT_EQ(*(logs[log_count_before_adding].GetDict().FindString("message")),
            "Element added: when the feature is enabled.");

  std::vector<policy::mojom::LogPtr> mojo_logs =
      GetLogsAsMojoList(policy_logger);
  EXPECT_EQ(mojo_logs.size(), log_count_before_adding + 1);
  EXPECT_EQ(mojo_logs[log_count_before_adding]->message,
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
  base::ListValue current_logs = GetLogsAsList(policy_logger);

  EXPECT_EQ(current_count, policy::PolicyLogger::kMaxLogCount);

  EXPECT_EQ(*(current_logs[0].GetDict().FindString("message")),
            "Element added: 1");

  EXPECT_EQ(*(current_logs[current_count - 1].GetDict().FindString("message")),
            "Element added: Last log added and size is exceeded.");
}

}  // namespace policy
