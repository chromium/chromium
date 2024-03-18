// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/policy/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Property;

namespace policy {

namespace {

void AddLogs(const std::string& message, PolicyLogger* policy_logger) {
  LOG_POLICY(INFO, POLICY_FETCHING) << "Element added: " << message;
}

}  // namespace

class PolicyLoggerTest : public PlatformTest {
 public:
  PolicyLoggerTest() = default;
  ~PolicyLoggerTest() override = default;

 protected:
  // Clears the logs list and resets the deletion flag before the test and its
  // tasks are deleted. This is important to prevent tests from affecting each
  // other's results.
  void SetUp() override {
    policy::PolicyLogger::GetInstance()->ResetLoggerForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Checks that the logger is enabled by feature and that `GetAsList` returns an
// updated list of logs.
TEST_F(PolicyLoggerTest, PolicyLoggingEnabled) {
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  size_t logs_size_before_adding = policy_logger->GetPolicyLogsSizeForTesting();
  AddLogs("when the feature is enabled.", policy_logger);

  EXPECT_EQ(policy_logger->GetAsList().size(), logs_size_before_adding + 1);
  EXPECT_EQ(*(policy_logger->GetAsList()[logs_size_before_adding]
                  .GetDict()
                  .FindString("message")),
            "Element added: when the feature is enabled.");
}

// Checks that the deletion of expired logs works as expected.
TEST_F(PolicyLoggerTest, DeleteOldLogs) {
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();
  policy_logger->EnableLogDeletion();
  size_t logs_size_before_adding = policy_logger->GetPolicyLogsSizeForTesting();

  AddLogs("First log at t=0.", policy_logger);
  AddLogs("Second log at t=0+delta.", policy_logger);

  base::TimeDelta first_time_elapsed = policy::PolicyLogger::kTimeToLive / 2;
  task_environment.FastForwardBy(first_time_elapsed + base::Minutes(1));
  AddLogs("Third log at t=TimeToLive/2.", policy_logger);

  // Check that the logs that were in the list for `kTimeToLive` minutes were
  // deleted and that the one that did not expire is still in the list.
  task_environment.FastForwardBy(first_time_elapsed);
  task_environment.RunUntilIdle();
  EXPECT_EQ(policy_logger->GetAsList().size(), size_t(1));
  EXPECT_EQ(*(policy_logger->GetAsList()[logs_size_before_adding]
                  .GetDict()
                  .FindString("message")),
            "Element added: Third log at t=TimeToLive/2.");

  // Check that the last log was deleted after `kTimeToLive` minutes to ensure
  // that a second deleting task was scheduled after deleting the old ones.
  task_environment.FastForwardBy(policy::PolicyLogger::kTimeToLive);
  task_environment.RunUntilIdle();
  EXPECT_EQ(policy_logger->GetAsList().size(), size_t(0));
}

// Checks that the first log  added is deleted when `PolicyLogger::kMaxLogSize`
// is exceeded.
TEST_F(PolicyLoggerTest, MaxSizeExceededDeletesOldestLog) {
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  AddLogs("First log that will be removed.", policy_logger);

  // Adds kMaxLogsSize` - 1 more elements until `kMaxLogsSize` is reached.
  for (int i = 0; i < static_cast<int>(policy::PolicyLogger::kMaxLogsSize) - 1;
       i++) {
    AddLogs(base::NumberToString(i + 1), policy_logger);
  }
  EXPECT_EQ(policy_logger->GetPolicyLogsSizeForTesting(),
            policy::PolicyLogger::kMaxLogsSize);

  AddLogs("Last log added and size is exceeded.", policy_logger);

  size_t current_size = policy_logger->GetPolicyLogsSizeForTesting();
  base::Value::List current_logs = policy_logger->GetAsList();

  EXPECT_EQ(current_size, policy::PolicyLogger::kMaxLogsSize);

  EXPECT_EQ(*(current_logs[0].GetDict().FindString("message")),
            "Element added: 1");

  EXPECT_EQ(*(current_logs[current_size - 1].GetDict().FindString("message")),
            "Element added: Last log added and size is exceeded.");
}

}  // namespace policy
