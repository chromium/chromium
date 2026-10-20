// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/origin_gating/core/decision.h"
#include "components/origin_gating/core/decision_attribution.h"
#include "components/origin_gating/core/decision_source.h"
#include "components/origin_gating/core/gateable_event.h"
#include "components/origin_gating/core/gating_decision.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/task_policy_config.h"
#include "components/origin_gating/core/task_policy_config_slot.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Pointer;
using ::testing::Return;

namespace origin_gating {
namespace {

using DecisionWithMetadata =
    OriginGatingChecker::Delegate::DecisionWithMetadata;

enum class TestCustomPredicate {
  kCustom1,
  kCustom2,
};

enum class OtherTestCustomPredicate {
  kOtherCustom1,
  kOtherCustom2,
};

MATCHER_P2(GatingDecisionIs, is_allowed, attribution, "") {
  return ExplainMatchResult(
      AllOf(Field("is_allowed", &GatingDecision::is_allowed, is_allowed),
            Field("attribution", &GatingDecision::attribution, attribution)),
      arg, result_listener);
}

enum class CheckerTestTool {
  kClick = 1,
  kNavigate = 2,
};

}  // namespace

template <>
const CustomPredicateDomain
    CustomPredicateDomain::kInstance<TestCustomPredicate>{};

template <>
const CustomPredicateDomain&
    CustomPredicateDomain::kInstance<OtherTestCustomPredicate>{};

template <>
const ToolDomain ToolDomain::kInstance<CheckerTestTool>{};

namespace {

TaskPolicyConfig::Rule ClicksAllowedRule() {
  return TaskPolicyConfig::Rule(
      /*navigation_sources=*/{},
      /*resources=*/{TaskPolicyConfig::Rule::Resource::kSession},
      /*allowed_tools=*/{ClientTool(CheckerTestTool::kClick)});
}

TaskPolicyConfig::Rule NavigationAllowedRule() {
  return TaskPolicyConfig::Rule(
      /*navigation_sources=*/{},
      /*resources=*/{TaskPolicyConfig::Rule::Resource::kSession},
      /*allowed_tools=*/{ClientTool(CheckerTestTool::kNavigate)});
}

class MockDelegate : public OriginGatingChecker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DoesOriginRequireUserConfirmation,
              (GatingDecisionContext * context,
               const GateableEvent& event,
               DoesOriginRequireUserConfirmationCallback callback),
              (const, override));
  MOCK_METHOD(void,
              EvaluateEnterprisePolicy,
              (const GURL& destination,
               EvaluateEnterprisePolicyCallback callback),
              (const, override));
  MOCK_METHOD(void,
              OnNoVerdict,
              (GatingDecisionContext * context,
               const GateableEvent& event,
               bool requires_user_confirmation,
               base::OnceCallback<void(NoVerdictResult)> callback),
              (override));

  base::WeakPtr<MockDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class OriginGatingCheckerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  NiceMock<MockDelegate> delegate_;

  void SetUpDelegateExpectations(const GateableEvent& event,
                                 bool requires_user_confirmation,
                                 bool is_allowed,
                                 bool did_prompt_user) {
    EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, event, _))
        .WillOnce(base::test::RunOnceCallback<2>(requires_user_confirmation));

    EXPECT_CALL(delegate_, OnNoVerdict(_, event, requires_user_confirmation, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            OriginGatingChecker::Delegate::NoVerdictResult{
                .is_allowed = is_allowed, .did_prompt_user = did_prompt_user}));
  }

  GatingDecision ComputeGatingDecisionAndVerifyAsynchrony(
      OriginGatingChecker& checker,
      std::unique_ptr<GatingDecisionContext> context,
      GateableEvent event) {
    base::test::TestFuture<std::unique_ptr<GatingDecisionContext>,
                           GatingDecision>
        future;
    checker.ComputeGatingDecision(std::move(context), std::move(event),
                                  future.GetCallback());

    EXPECT_FALSE(future.IsReady());
    return future.Get<1>();
  }
};

TEST_F(OriginGatingCheckerTest, FallsBack_Allowed_NoPrompt) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, FallsBack_Allowed_WithPrompt) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/true);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, FallsBack_Blocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, FallsBack_Blocked_WithPrompt) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/true,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/true);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
}

class TestGatingContext : public GatingDecisionContext {
 public:
  explicit TestGatingContext(int val) : value(val) {}
  ~TestGatingContext() override = default;
  int value;
};

TEST_F(OriginGatingCheckerTest, PlumbsContext) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  auto context = std::make_unique<TestGatingContext>(42);
  GatingDecisionContext* expected_context_ptr = context.get();

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             expected_context_ptr,
                             GateableEvent(NavigationResponseEvent{
                                 .source = source, .destination = destination}),
                             _))
      .WillOnce(base::test::RunOnceCallback<2>(true));

  EXPECT_CALL(delegate_,
              OnNoVerdict(expected_context_ptr,
                          GateableEvent(NavigationResponseEvent{
                              .source = source, .destination = destination}),
                          /*requires_user_confirmation=*/true, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, std::move(context),
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowSameOrigin_ShortCircuits) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowSameOrigin,
            {GateableEvent::kNavigationRequest,
             GateableEvent::kNavigationResponse}}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com/page1");
  GURL destination("https://example.com/page2");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kAllowSameOrigin));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowSameOrigin_NoDecision_FallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowSameOrigin,
            {GateableEvent::kNavigationRequest,
             GateableEvent::kNavigationResponse}}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_Blocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kForbidNonLocalhostIpAddress,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://192.168.1.1/page");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kForbidNonLocalhostIpAddress));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_Ipv4LocalhostFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kForbidNonLocalhostIpAddress,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://127.0.0.1/page");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_Ipv6LocalhostFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kForbidNonLocalhostIpAddress,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://[::1]/page");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_NoDecision_FallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kForbidNonLocalhostIpAddress,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_HttpsFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_HttpBlocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kRequireHttpsOrLocalhost));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_DomainLocalhostFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://localhost/page");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_Ipv4LocalhostFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://127.0.0.1/page");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_Ipv6LocalhostFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://[::1]/page");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_NonHttpLocalhostBlocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("file://localhost/tmp");

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kRequireHttpsOrLocalhost));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrHttp_HttpsFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrHttp, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrHttp_HttpFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrHttp, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrHttp_NonWebSchemeBlocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrHttp, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("file:///tmp/file");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kRequireHttpsOrHttp));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowHttpLocalhost_HttpLocalhostAllowed) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowHttpLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://localhost/path");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kAllowHttpLocalhost));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowHttpLocalhost_NonHttpSchemeFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowHttpLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("ws://localhost/path");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowHttpLocalhost_NonLocalhostFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowHttpLocalhost, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("http://foo.com/path");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowAboutBlank_AboutBlankAllowed) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowAboutBlank, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("about:blank");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kAllowAboutBlank));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowAboutBlank_NonBlankFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowAboutBlank, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_NoConfigFallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kBlockByTaskPolicyConfig, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_NavigationAllowed) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowByTaskPolicyConfig, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  // Configure the slot on the checker.
  checker->task_policy_config_slot().Assign(TaskPolicyConfig(
      {{
          {TaskPolicyConfig::Location(TaskPolicyConfig::Wildcard()),
           NavigationAllowedRule()},
      }},
      ClientTool(CheckerTestTool::kNavigate)));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  // Since it is explicitly allowed, we don't query the delegate for user
  // confirmation or no-verdict (it does not fall back).
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kAllowByTaskPolicyConfig));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_NavigationBlocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kBlockByTaskPolicyConfig, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  // Set an empty config which blocks all navigations.
  checker->task_policy_config_slot().Assign(TaskPolicyConfig());

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kBlockByTaskPolicyConfig));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_PageActionAllowed) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kAllowByTaskPolicyConfig, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  // Configure the slot to allow actuation.
  checker->task_policy_config_slot().Assign(TaskPolicyConfig(
      {{
          {TaskPolicyConfig::Location(TaskPolicyConfig::Wildcard()),
           ClicksAllowedRule()},
      }},
      ClientTool(CheckerTestTool::kNavigate)));

  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  // Use page action event.
  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(PageActionEvent{
                      .destination = destination,
                      .tool = ClientTool(CheckerTestTool::kClick)})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kAllowByTaskPolicyConfig));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_PageActionBlocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kBlockByTaskPolicyConfig, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  // Set an empty config which blocks all actuations.
  checker->task_policy_config_slot().Assign(TaskPolicyConfig());

  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(PageActionEvent{
                      .destination = destination,
                      .tool = ClientTool(CheckerTestTool::kClick)})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kBlockByTaskPolicyConfig));
}

// TaskPolicyConfig usage doesn't necessarily preclude blockage by other
// predicates, even if the task policy config predicate is listed first in the
// predicate order.
TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_InterleavedPredicates) {
  const GURL kAllowedByConfigAndListSite("https://allowedbyboth.com");
  const GURL kBlockedByConfigAndListSite("https://blockedbyboth.com");
  const GURL kAllowedByConfigIgnoredByListSite("https://allowedbyconfig.com");
  const GURL kBlockedByConfigSite("https://blockedbyconfig.com");
  const GURL kBlockedByListSite("https://blockedbylist.com");

  CustomPredicate custom(
      base::BindLambdaForTesting(
          [&](GatingDecisionContext*, const GateableEvent& event) -> Decision {
            const GURL& destination = event.destination();
            if (destination == kAllowedByConfigAndListSite) {
              return Decision::kAllowed;
            }
            if (destination == kBlockedByConfigSite ||
                destination == kAllowedByConfigIgnoredByListSite) {
              return Decision::kNoDecision;
            }
            if (destination == kBlockedByConfigAndListSite ||
                destination == kBlockedByListSite) {
              return Decision::kBlocked;
            }
            NOTREACHED();
          }),
      TestCustomPredicate::kCustom1);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kBlockByTaskPolicyConfig,
               GateableEventSet::All()},
              {custom, GateableEventSet::All()},
              {DecisionSource::kAllowByTaskPolicyConfig,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  checker->task_policy_config_slot().Assign(TaskPolicyConfig(
      /*location_rules=*/
      {
          // Allowed on the below sites, blocked elsewhere.
          {TaskPolicyConfig::Location(
               net::SchemefulSite(kAllowedByConfigAndListSite)),
           NavigationAllowedRule()},
          {TaskPolicyConfig::Location(
               net::SchemefulSite(kAllowedByConfigIgnoredByListSite)),
           NavigationAllowedRule()},
          {TaskPolicyConfig::Location(net::SchemefulSite(kBlockedByListSite)),
           NavigationAllowedRule()},
      },
      ClientTool(CheckerTestTool::kNavigate)));

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(
              NavigationResponseEvent{.source = kBlockedByListSite,
                                      .destination = kBlockedByListSite})),
      GatingDecisionIs(/*is_allowed*/ false, TestCustomPredicate::kCustom1));
  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = kBlockedByConfigSite,
                      .destination = kBlockedByConfigSite})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kBlockByTaskPolicyConfig));
  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = kAllowedByConfigIgnoredByListSite,
                      .destination = kAllowedByConfigIgnoredByListSite})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kAllowByTaskPolicyConfig));
  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{
              .source = kAllowedByConfigAndListSite,
              .destination = kAllowedByConfigAndListSite})),
      GatingDecisionIs(/*is_allowed*/ true, TestCustomPredicate::kCustom1));
  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = kBlockedByConfigAndListSite,
                      .destination = kBlockedByConfigAndListSite})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kBlockByTaskPolicyConfig));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_TaskPolicyConfig_PageActionToolGating) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kBlockByTaskPolicyConfig,
               GateableEventSet::All()},
              {DecisionSource::kAllowByTaskPolicyConfig,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  // Configure the slot to allow kClick tool only.
  checker->task_policy_config_slot().Assign(TaskPolicyConfig(
      {{
          {TaskPolicyConfig::Location(TaskPolicyConfig::Wildcard()),
           TaskPolicyConfig::Rule(
               /*navigation_sources=*/{},
               /*resources=*/{TaskPolicyConfig::Rule::Resource::kSession},
               /*allowed_tools=*/{ClientTool(CheckerTestTool::kClick)})},
      }},
      ClientTool(CheckerTestTool::kNavigate)));

  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  // Allowed tool: kClick
  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(PageActionEvent{
                      .destination = destination,
                      .tool = ClientTool(CheckerTestTool::kClick)})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kAllowByTaskPolicyConfig));

  // Blocked tool: kNavigate
  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(PageActionEvent{
                      .destination = destination,
                      .tool = ClientTool(CheckerTestTool::kNavigate)})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kBlockByTaskPolicyConfig));
}

TEST_F(OriginGatingCheckerTest, BuiltInPredicate_EnterprisePolicy_Allowed) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kEnterprisePolicy, GateableEventSet::All()},
           {DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  GateableEvent event(
      NavigationResponseEvent{.source = source, .destination = destination});

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(DecisionWithMetadata{
          .decision = Decision::kAllowed, .bypass_cache = true}));
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kEnterprisePolicy));

  // With bypass_cache set, the allow decision must not be persisted. A
  // subsequent evaluation where enterprise policy makes no decision should miss
  // the cache and fall back to the delegate.
  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          DecisionWithMetadata{.decision = Decision::kNoDecision}));
  SetUpDelegateExpectations(event,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_EnterprisePolicy_Allowed_PersistsCache) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kEnterprisePolicy, GateableEventSet::All()},
           {DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  GateableEvent event(
      NavigationResponseEvent{.source = source, .destination = destination});

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(DecisionWithMetadata{
          .decision = Decision::kAllowed, .bypass_cache = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kEnterprisePolicy));

  // Without bypass_cache, the allow decision must be persisted. A subsequent
  // evaluation where enterprise policy makes no decision should hit the cache.
  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          DecisionWithMetadata{.decision = Decision::kNoDecision}));
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, event, _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*requires_user_confirmation=*/false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ true,
                       DecisionSource::kCacheWithoutUserConfirmation));
}

TEST_F(
    OriginGatingCheckerTest,
    BuiltInPredicate_EnterprisePolicy_Allowed_DoesNotPersistCacheWithoutCachePredicate) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kEnterprisePolicy, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(DecisionWithMetadata{
          .decision = Decision::kAllowed, .bypass_cache = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kEnterprisePolicy));
  // Without a cache predicate in the configuration, the allow decision must not
  // be persisted to the cache even when bypass_cache is false.
  OriginGatingCache::SizeMetrics metrics = checker->GetCacheSizeMetrics();
  EXPECT_EQ(metrics.allow_list_size, 0u);
  EXPECT_EQ(metrics.confirmed_list_size, 0u);
}

TEST_F(OriginGatingCheckerTest, BuiltInPredicate_EnterprisePolicy_Blocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kEnterprisePolicy, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          DecisionWithMetadata{.decision = Decision::kBlocked}));
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ false,
                               DecisionSource::kEnterprisePolicy));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_EnterprisePolicy_NoDecision_FallsBack) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kEnterprisePolicy, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          DecisionWithMetadata{.decision = Decision::kNoDecision}));
  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, AsyncCustomPredicate_Allowed_ShortCircuits) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent& event,
                             base::OnceCallback<void(Decision)> callback) {
        EXPECT_THAT(event.source(),
                    testing::Pointee(GURL("https://example.com")));
        EXPECT_EQ(event.destination(), GURL("https://foo.com"));
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom1);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{custom, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, TestCustomPredicate::kCustom1));
}

TEST_F(OriginGatingCheckerTest,
       CustomPredicate_AttributionDoesNotMatchDifferentEnumTypeWithSameValue) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&) {
        return Decision::kAllowed;
      }),
      TestCustomPredicate::kCustom1);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{custom, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      *checker, nullptr,
      GateableEvent(
          NavigationResponseEvent{.source = GURL("https://example.com"),
                                  .destination = GURL("https://foo.com")}));

  EXPECT_THAT(decision, GatingDecisionIs(/*is_allowed*/ true,
                                         TestCustomPredicate::kCustom1));
  EXPECT_EQ(decision.attribution.CustomPredicateId<TestCustomPredicate>(),
            TestCustomPredicate::kCustom1);

  EXPECT_NE(decision.attribution, OtherTestCustomPredicate::kOtherCustom1);
  EXPECT_CHECK_DEATH(
      decision.attribution.CustomPredicateId<OtherTestCustomPredicate>());
}

TEST_F(OriginGatingCheckerTest,
       AsyncCustomPredicate_NoDecision_FallsBackToDelegate) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kNoDecision);
      }),
      TestCustomPredicate::kCustom1);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{custom, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, SyncCustomPredicate_Allowed_ShortCircuits) {
  CustomPredicate custom(
      base::BindRepeating(
          [](GatingDecisionContext*, const GateableEvent& event) {
            EXPECT_THAT(event.source(),
                        testing::Pointee(GURL("https://example.com")));
            EXPECT_EQ(event.destination(), GURL("https://foo.com"));
            return Decision::kAllowed;
          }),
      TestCustomPredicate::kCustom2);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{custom, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, TestCustomPredicate::kCustom2));
}

TEST_F(OriginGatingCheckerTest,
       SyncCustomPredicate_NoDecision_FallsBackToDelegate) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&) {
        return Decision::kNoDecision;
      }),
      TestCustomPredicate::kCustom2);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{custom, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(GateableEvent(NavigationResponseEvent{
                                .source = source, .destination = destination}),
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       CacheHit_UserConfirmedOrigin_ShortCircuitsImmediately) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kCacheWithUserConfirmation,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker->AllowNavigationTo(destination_origin, /*is_user_confirmed=*/true);

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kCacheWithUserConfirmation));
}

TEST_F(
    OriginGatingCheckerTest,
    CacheHit_UserConfirmedOrigin_SameSiteSubdomain_ShortCircuitsEvenWithOriginScope) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kCacheWithUserConfirmation,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://sub.foo.com");
  url::Origin confirmed_origin = url::Origin::Create(GURL("https://foo.com"));

  checker->AllowNavigationTo(confirmed_origin, /*is_user_confirmed=*/true);

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kCacheWithUserConfirmation));
}

TEST_F(OriginGatingCheckerTest,
       CacheMiss_NonConfirmedOrigin_SensitiveDestination_QueriesDelegate) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kCacheWithUserConfirmation,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker->AllowNavigationTo(destination_origin, /*is_user_confirmed=*/false);

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             _,
                             GateableEvent(NavigationResponseEvent{
                                 .source = source, .destination = destination}),
                             _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*requires_user_confirmation=*/true));

  EXPECT_CALL(delegate_,
              OnNoVerdict(_,
                          GateableEvent(NavigationResponseEvent{
                              .source = source, .destination = destination}),
                          /*requires_user_confirmation=*/true, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = true}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest,
       CacheHit_NonConfirmedOrigin_NonSensitiveDestination_ShortCircuits) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kCacheWithUserConfirmation,
               GateableEventSet::All()},
              {DecisionSource::kCacheWithoutUserConfirmation,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker->AllowNavigationTo(destination_origin, /*is_user_confirmed=*/false);

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             _,
                             GateableEvent(NavigationResponseEvent{
                                 .source = source, .destination = destination}),
                             _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*requires_user_confirmation=*/false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(NavigationResponseEvent{
                      .source = source, .destination = destination})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kCacheWithoutUserConfirmation));
}

TEST_F(OriginGatingCheckerTest,
       CacheWithoutUserConfirmation_PageAction_ShortCircuits) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {DecisionSource::kCacheWithoutUserConfirmation,
               GateableEventSet::All()},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker->AllowNavigationTo(destination_origin, /*is_user_confirmed=*/false);

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             _,
                             GateableEvent(PageActionEvent{
                                 .destination = destination,
                                 .tool = ClientTool(CheckerTestTool::kClick)}),
                             _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*requires_user_confirmation=*/false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(ComputeGatingDecisionAndVerifyAsynchrony(
                  *checker, nullptr,
                  GateableEvent(PageActionEvent{
                      .destination = destination,
                      .tool = ClientTool(CheckerTestTool::kClick)})),
              GatingDecisionIs(/*is_allowed*/ true,
                               DecisionSource::kCacheWithoutUserConfirmation));
}

TEST_F(OriginGatingCheckerTest, PredicateSkipped_WhenEventNotApplicable) {
  // A custom predicate that would allow, but is restricted to kPageAction only.
  CustomPredicate page_action_only(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom2);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {page_action_only, {GateableEvent::kPageAction}},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  // For a navigation-request event the predicate is skipped, so the checker
  // falls back to the delegate.
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             _,
                             GateableEvent(NavigationRequestEvent{
                                 .source = source, .destination = destination}),
                             _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_,
              OnNoVerdict(_,
                          GateableEvent(NavigationRequestEvent{
                              .source = source, .destination = destination}),
                          false, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = false, .did_prompt_user = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationRequestEvent{.source = source,
                                               .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, PredicateRuns_WhenEventApplicable) {
  CustomPredicate page_action_only(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom2);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {
              {page_action_only, {GateableEvent::kPageAction}},
          },
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(
              PageActionEvent{.destination = destination,
                              .tool = ClientTool(CheckerTestTool::kClick)})),
      GatingDecisionIs(/*is_allowed*/ true, TestCustomPredicate::kCustom2));
}

TEST_F(OriginGatingCheckerTest, EventReachesPredicateAndDelegate) {
  // The custom predicate returns kNoDecision so evaluation reaches the
  // delegate, allowing us to assert the event is threaded through both hops.
  CustomPredicate observing_predicate(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kNoDecision);
      }),
      TestCustomPredicate::kCustom2);

  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{observing_predicate, GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             _,
                             GateableEvent(PageActionEvent{
                                 .destination = destination,
                                 .tool = ClientTool(CheckerTestTool::kClick)}),
                             _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_,
              OnNoVerdict(_,
                          GateableEvent(PageActionEvent{
                              .destination = destination,
                              .tool = ClientTool(CheckerTestTool::kClick)}),
                          false, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(
              PageActionEvent{.destination = destination,
                              .tool = ClientTool(CheckerTestTool::kClick)})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, BypassCache_SuppressesCacheWrite) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  GateableEvent event(
      NavigationResponseEvent{.source = source, .destination = destination});

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, event, _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, event, false, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true,
              .did_prompt_user = false,
              .bypass_cache = true}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
  // The allow decision must not have been persisted. A subsequent evaluation
  // should miss the cache and fall back to the delegate.
  SetUpDelegateExpectations(event,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, NoBypassCache_PersistsCacheWrite) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  GateableEvent event(
      NavigationResponseEvent{.source = source, .destination = destination});

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, event, _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, event, false, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true,
              .did_prompt_user = false,
              .bypass_cache = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
  // The allow decision must have been persisted. A subsequent evaluation should
  // hit the cache.
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, event, _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _)).Times(0);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ true,
                       DecisionSource::kCacheWithoutUserConfirmation));
}

TEST_F(OriginGatingCheckerTest,
       NoVerdict_Allowed_DoesNotPersistCacheWithoutCachePredicate) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             _,
                             GateableEvent(NavigationResponseEvent{
                                 .source = source, .destination = destination}),
                             _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_,
              OnNoVerdict(_,
                          GateableEvent(NavigationResponseEvent{
                              .source = source, .destination = destination}),
                          false, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true,
              .did_prompt_user = false,
              .bypass_cache = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(
          *checker, nullptr,
          GateableEvent(NavigationResponseEvent{.source = source,
                                                .destination = destination})),
      GatingDecisionIs(/*is_allowed*/ true, DecisionSource::kNoVerdict));
  // Without a cache predicate in the configuration, the allow decision must not
  // be persisted to the cache even when bypass_cache is false.
  OriginGatingCache::SizeMetrics metrics = checker->GetCacheSizeMetrics();
  EXPECT_EQ(metrics.allow_list_size, 0u);
  EXPECT_EQ(metrics.confirmed_list_size, 0u);
}

TEST_F(OriginGatingCheckerTest,
       AllowNavigationTo_DoesNotPersistCacheWithoutCachePredicate) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {}, OriginGatingConfiguration::CacheScope::kOrigin));

  url::Origin destination_origin = url::Origin::Create(GURL("https://foo.com"));

  checker->AllowNavigationTo(destination_origin, /*is_user_confirmed=*/true);
  OriginGatingCache::SizeMetrics metrics = checker->GetCacheSizeMetrics();
  EXPECT_EQ(metrics.allow_list_size, 0u);
  EXPECT_EQ(metrics.confirmed_list_size, 0u);

  checker->AllowNavigationTo({destination_origin});
  metrics = checker->GetCacheSizeMetrics();
  EXPECT_EQ(metrics.allow_list_size, 0u);
  EXPECT_EQ(metrics.confirmed_list_size, 0u);
}

TEST_F(OriginGatingCheckerTest, GetCacheSizeMetrics) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  OriginGatingCache::SizeMetrics metrics = checker->GetCacheSizeMetrics();
  EXPECT_EQ(metrics.allow_list_size, 0u);
  EXPECT_EQ(metrics.confirmed_list_size, 0u);

  checker->AllowNavigationTo(url::Origin::Create(GURL("https://foo.com")),
                             /*is_user_confirmed=*/true);
  checker->AllowNavigationTo(url::Origin::Create(GURL("https://bar.com")),
                             /*is_user_confirmed=*/false);

  metrics = checker->GetCacheSizeMetrics();
  EXPECT_EQ(metrics.allow_list_size, 2u);
  EXPECT_EQ(metrics.confirmed_list_size, 1u);
}

TEST_F(OriginGatingCheckerTest, BypassCache_IgnoredWhenBlocked) {
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate_.GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  GateableEvent event(
      NavigationResponseEvent{.source = source, .destination = destination});

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, event, _))
      .WillOnce(base::test::RunOnceCallback<2>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, event, false, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = false,
              .did_prompt_user = false,
              .bypass_cache = false}));

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
  // A blocked decision is never persisted regardless of bypass_cache. A
  // subsequent evaluation should miss the cache and fall back to the delegate.
  SetUpDelegateExpectations(event,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/false);

  EXPECT_THAT(
      ComputeGatingDecisionAndVerifyAsynchrony(*checker, nullptr, event),
      GatingDecisionIs(/*is_allowed*/ false, DecisionSource::kNoVerdict));
}

TEST_F(OriginGatingCheckerTest, DelegateDestroyedBeforeEvaluation) {
  auto delegate = std::make_unique<NiceMock<MockDelegate>>();
  auto checker = OriginGatingChecker::CreateForTesting(
      delegate->GetWeakPtr(),
      OriginGatingConfiguration(
          {{DecisionSource::kCacheWithoutUserConfirmation,
            GateableEventSet::All()}},
          OriginGatingConfiguration::CacheScope::kOrigin));

  // Destroy the delegate before computing decision.
  delegate.reset();

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker->ComputeGatingDecision(
      nullptr,
      GateableEvent(NavigationResponseEvent{
          .source = GURL("https://example.com"),
          .destination = GURL("https://destination.com")}),
      future.GetCallback());

  // If the delegate was destroyed, the callback is not executed.
  EXPECT_FALSE(future.IsReady());
}

}  // namespace
}  // namespace origin_gating
