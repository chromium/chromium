// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  void TearDown() override {
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

  constexpr char kTimestampRegex[] =
      R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} (AM|PM))";
  base::ListValue logs = GetLogsAsList(policy_logger);
  EXPECT_EQ(logs.size(), log_count_before_adding + 1);
  const base::DictValue& dict = logs[log_count_before_adding].GetDict();
  EXPECT_EQ(*(dict.FindString("message")),
            "Element added: when the feature is enabled.");
  ASSERT_NE(dict.FindString("timestamp"), nullptr);
  EXPECT_THAT(*dict.FindString("timestamp"),
              testing::MatchesRegex(kTimestampRegex));

  std::vector<policy::mojom::LogPtr> mojo_logs =
      GetLogsAsMojoList(policy_logger);
  EXPECT_EQ(mojo_logs.size(), log_count_before_adding + 1);
  EXPECT_EQ(mojo_logs[log_count_before_adding]->message,
            "Element added: when the feature is enabled.");
  EXPECT_THAT(mojo_logs[log_count_before_adding]->timestamp,
              testing::MatchesRegex(kTimestampRegex));
}

// Checks that without compression enabled, oldest log is deleted when
// `PolicyLogger::kMaxUncompressedLogCount` is exceeded.
TEST_F(PolicyLoggerTest, MaxCountExceededWithoutCompressionDeletesOldestLog) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  AddLogs("First log that will be removed.", policy_logger);

  // Adds `kMaxUncompressedLogCount` - 1 more elements until
  // `kMaxUncompressedLogCount` is reached.
  for (size_t i = 0; i < policy::PolicyLogger::kMaxUncompressedLogCount - 1;
       i++) {
    AddLogs(base::NumberToString(i + 1), policy_logger);
  }
  EXPECT_EQ(GetLogCount(policy_logger),
            policy::PolicyLogger::kMaxUncompressedLogCount);

  AddLogs("Last log added and size is exceeded.", policy_logger);

  size_t current_count = GetLogCount(policy_logger);
  base::ListValue current_logs = GetLogsAsList(policy_logger);

  EXPECT_EQ(current_count, policy::PolicyLogger::kMaxUncompressedLogCount);

  EXPECT_EQ(*(current_logs[0].GetDict().FindString("message")),
            "Element added: 1");

  EXPECT_EQ(*(current_logs[current_count - 1].GetDict().FindString("message")),
            "Element added: Last log added and size is exceeded.");
}

// Checks that with compression enabled, logs are compressed when
// `PolicyLogger::kMaxUncompressedLogCount` is reached.
TEST_F(PolicyLoggerTest, MaxCountExceededWithCompressionClearsMemory) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();
  policy_logger->EnableLogCompression(
      task_environment_.GetMainThreadTaskRunner());

  for (size_t i = 0; i < policy::PolicyLogger::kMaxUncompressedLogCount; i++) {
    AddLogs(base::NumberToString(i), policy_logger);
  }

  base::ListValue current_logs = GetLogsAsList(policy_logger);
  EXPECT_EQ(current_logs.size(),
            policy::PolicyLogger::kMaxUncompressedLogCount);
  EXPECT_EQ(*(current_logs[0].GetDict().FindString("message")),
            "Element added: 0");
  EXPECT_EQ(
      *(current_logs[policy::PolicyLogger::kMaxUncompressedLogCount - 1]
            .GetDict()
            .FindString("message")),
      base::StrCat({"Element added: ",
                    base::NumberToString(
                        policy::PolicyLogger::kMaxUncompressedLogCount - 1)}));
}

// Checks that compressed logs are prepended to subsequent in-memory logs.
TEST_F(PolicyLoggerTest, CompressedLogsPrependedToNewInMemoryLogs) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();
  policy_logger->EnableLogCompression(
      task_environment_.GetMainThreadTaskRunner());

  for (size_t i = 0; i < policy::PolicyLogger::kMaxUncompressedLogCount; i++) {
    AddLogs(base::NumberToString(i), policy_logger);
  }

  // Add 2 more logs to memory.
  AddLogs("New in-memory log 1", policy_logger);
  AddLogs("New in-memory log 2", policy_logger);

  base::ListValue current_logs = GetLogsAsList(policy_logger);
  EXPECT_EQ(current_logs.size(),
            policy::PolicyLogger::kMaxUncompressedLogCount + 2);
  EXPECT_EQ(*(current_logs[0].GetDict().FindString("message")),
            "Element added: 0");
  EXPECT_EQ(*(current_logs[policy::PolicyLogger::kMaxUncompressedLogCount]
                  .GetDict()
                  .FindString("message")),
            "Element added: New in-memory log 1");
  EXPECT_EQ(*(current_logs[policy::PolicyLogger::kMaxUncompressedLogCount + 1]
                  .GetDict()
                  .FindString("message")),
            "Element added: New in-memory log 2");
}

// Checks that when `PolicyLogger::kMaxCompressedLogCount` is exceeded, the
// oldest compressed logs are deleted.
TEST_F(PolicyLoggerTest, MaxCompressedLogCountExceededDeletesOldestLogs) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();
  policy_logger->EnableLogCompression(
      task_environment_.GetMainThreadTaskRunner());

  constexpr size_t kExtraLogCount =
      policy::PolicyLogger::kMaxUncompressedLogCount;
  for (size_t i = 0;
       i < policy::PolicyLogger::kMaxCompressedLogCount + kExtraLogCount; i++) {
    AddLogs(base::NumberToString(i), policy_logger);
  }

  base::ListValue current_logs = GetLogsAsList(policy_logger);
  EXPECT_EQ(current_logs.size(), policy::PolicyLogger::kMaxCompressedLogCount);
  EXPECT_EQ(
      *(current_logs[0].GetDict().FindString("message")),
      base::StrCat({"Element added: ", base::NumberToString(kExtraLogCount)}));
  EXPECT_EQ(*(current_logs[policy::PolicyLogger::kMaxCompressedLogCount - 1]
                  .GetDict()
                  .FindString("message")),
            base::StrCat({"Element added: ",
                          base::NumberToString(
                              policy::PolicyLogger::kMaxCompressedLogCount +
                              kExtraLogCount - 1)}));
}

// Checks that logs containing newlines and quotes serialize and deserialize
// accurately.
TEST_F(PolicyLoggerTest, HandlesSpecialCharactersAndNewlines) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!PolicyLogger::IsPolicyLoggingEnabled()) {
    GTEST_SKIP() << "Policy logging is disabled on ChromeOS stable";
  }
#endif
  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();
  policy_logger->EnableLogCompression(
      task_environment_.GetMainThreadTaskRunner());

  std::string multiline_message = "Line 1\nLine 2\n{\"key\": \"value\"}\t\r\n";
  AddLogs(multiline_message, policy_logger);

  // Fill up to trigger compression.
  for (size_t i = 1; i < policy::PolicyLogger::kMaxUncompressedLogCount; i++) {
    AddLogs(base::NumberToString(i), policy_logger);
  }

  constexpr char kTimestampRegex[] =
      R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} (AM|PM))";
  base::ListValue current_logs = GetLogsAsList(policy_logger);
  EXPECT_EQ(current_logs.size(),
            policy::PolicyLogger::kMaxUncompressedLogCount);
  const base::DictValue& dict = current_logs[0].GetDict();
  EXPECT_EQ(*dict.FindString("message"),
            base::StrCat({"Element added: ", multiline_message}));
  EXPECT_EQ(*dict.FindString("logSeverity"), "INFO");
  EXPECT_EQ(*dict.FindString("logSource"), "Policy Fetching");
  EXPECT_EQ(*dict.FindString("fileAndLine"), "policy_logger_unittest.cc:22");
  ASSERT_NE(dict.FindString("timestamp"), nullptr);
  EXPECT_THAT(*dict.FindString("timestamp"),
              testing::MatchesRegex(kTimestampRegex));
}

}  // namespace policy
