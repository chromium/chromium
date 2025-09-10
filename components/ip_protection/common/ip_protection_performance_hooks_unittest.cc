// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_performance_hooks.h"

#include <string>
#include <vector>

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
  base::test::TaskEnvironment task_environment_;
};

TEST_F(IpProtectionPerformanceHooksTest, GetInitialData) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnGetInitialDataStart();
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
}

TEST_F(IpProtectionPerformanceHooksTest, GenerateBlindedTokenRequests) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnGenerateBlindedTokenRequestsStart();
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
}

TEST_F(IpProtectionPerformanceHooksTest, AuthAndSign) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnAuthAndSignStart();
  hooks.OnAuthAndSignEnd();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto query_result =
      ttp.RunQuery("SELECT name FROM slice WHERE category = 'ip_protection'");
  ASSERT_TRUE(query_result.has_value());
  EXPECT_THAT(query_result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"AuthAndSign"}));
}

TEST_F(IpProtectionPerformanceHooksTest, UnblindTokens) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("ip_protection");

  IpProtectionPerformanceHooks hooks(perfetto::Track(123));
  hooks.OnUnblindTokensStart();
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
}

}  // namespace
}  // namespace ip_protection
