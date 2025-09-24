// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_performance_hooks.h"

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace ip_protection {
namespace {

class IpProtectionPerformanceHooksTest : public testing::Test {
 public:
  IpProtectionPerformanceHooksTest() = default;

  base::test::TracingEnvironment tracing_environment_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(IpProtectionPerformanceHooksTest, GetInitialData) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");
  base::HistogramTester histogram_tester;

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnGetInitialDataStart();
  task_environment_.FastForwardBy(base::Seconds(1));
  hooks.OnGetInitialDataEnd();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto query_result =
      ttp.RunQuery("SELECT name FROM slice WHERE category = 'ip_protection'");
  ASSERT_TRUE(query_result.has_value());
  EXPECT_THAT(
      query_result.value(),
      ::testing::ElementsAre(std::vector<std::string>{"name"},
                             std::vector<std::string>{"GetInitialData"}));
  histogram_tester.ExpectUniqueTimeSample(
      "NetworkService.IpProtection.TokenBatchGenerationTime.GetInitialData",
      base::Seconds(1), 1);
}

TEST_F(IpProtectionPerformanceHooksTest, GenerateBlindedTokenRequests) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");
  base::HistogramTester histogram_tester;

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnGenerateBlindedTokenRequestsStart();
  task_environment_.FastForwardBy(base::Seconds(2));
  hooks.OnGenerateBlindedTokenRequestsEnd();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto query_result =
      ttp.RunQuery("SELECT name FROM slice WHERE category = 'ip_protection'");
  ASSERT_TRUE(query_result.has_value());
  EXPECT_THAT(query_result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"name"},
                  std::vector<std::string>{"GenerateBlindedTokenRequests"}));
  histogram_tester.ExpectUniqueTimeSample(
      "NetworkService.IpProtection.TokenBatchGenerationTime."
      "GenerateBlindedTokenRequests",
      base::Seconds(2), 1);
}

TEST_F(IpProtectionPerformanceHooksTest, AuthAndSign) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");
  base::HistogramTester histogram_tester;

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnAuthAndSignStart();
  task_environment_.FastForwardBy(base::Seconds(3));
  hooks.OnAuthAndSignEnd();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto query_result =
      ttp.RunQuery("SELECT name FROM slice WHERE category = 'ip_protection'");
  ASSERT_TRUE(query_result.has_value());
  EXPECT_THAT(query_result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"AuthAndSign"}));
  histogram_tester.ExpectUniqueTimeSample(
      "NetworkService.IpProtection.TokenBatchGenerationTime.AuthAndSign",
      base::Seconds(3), 1);
}

TEST_F(IpProtectionPerformanceHooksTest, UnblindTokens) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");
  base::HistogramTester histogram_tester;

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnUnblindTokensStart();
  task_environment_.FastForwardBy(base::Seconds(4));
  hooks.OnUnblindTokensEnd();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto query_result =
      ttp.RunQuery("SELECT name FROM slice WHERE category = 'ip_protection'");
  ASSERT_TRUE(query_result.has_value());
  EXPECT_THAT(
      query_result.value(),
      ::testing::ElementsAre(std::vector<std::string>{"name"},
                             std::vector<std::string>{"UnblindTokens"}));
  histogram_tester.ExpectUniqueTimeSample(
      "NetworkService.IpProtection.TokenBatchGenerationTime.UnblindTokens",
      base::Seconds(4), 1);
}

}  // namespace
}  // namespace ip_protection
