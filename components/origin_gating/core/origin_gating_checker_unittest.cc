// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/origin_gating/core/actor_container_config.h"
#include "components/origin_gating/core/actor_container_config_slot.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
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

}  // namespace

template <>
const CustomPredicateDomain
    CustomPredicateDomain::kInstance<TestCustomPredicate>{};

template <>
const CustomPredicateDomain&
    CustomPredicateDomain::kInstance<OtherTestCustomPredicate>{};

namespace {

class MockDelegate : public OriginGatingChecker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DoesOriginRequireUserConfirmation,
              (GatingDecisionContext * context,
               GateableEvent event,
               const GURL& source,
               const GURL& destination,
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
               GateableEvent event,
               const GURL& source,
               const GURL& destination,
               bool requires_user_confirmation,
               base::OnceCallback<void(NoVerdictResult)> callback),
              (override));
};

class OriginGatingCheckerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  NiceMock<MockDelegate> delegate_;

  void SetUpDelegateExpectations(const GURL& source,
                                 const GURL& destination,
                                 bool requires_user_confirmation,
                                 bool is_allowed,
                                 bool did_prompt_user) {
    EXPECT_CALL(delegate_,
                DoesOriginRequireUserConfirmation(_, _, source, destination, _))
        .WillOnce(base::test::RunOnceCallback<4>(requires_user_confirmation));

    EXPECT_CALL(delegate_, OnNoVerdict(_, _, source, destination,
                                       requires_user_confirmation, _))
        .WillOnce(base::test::RunOnceCallback<5>(
            OriginGatingChecker::Delegate::NoVerdictResult{
                .is_allowed = is_allowed, .did_prompt_user = did_prompt_user}));
  }

  GatingDecision ComputeGatingDecisionAndVerifyAsynchrony(
      OriginGatingChecker& checker,
      std::unique_ptr<GatingDecisionContext> context,
      const GURL& source,
      const GURL& destination) {
    base::test::TestFuture<std::unique_ptr<GatingDecisionContext>,
                           GatingDecision>
        future;
    checker.ComputeGatingDecision(std::move(context),
                                  GateableEvent::kNavigationResponse, source,
                                  destination, future.GetCallback());

    EXPECT_FALSE(future.IsReady());
    return future.Get<1>();
  }
};

TEST_F(OriginGatingCheckerTest, FallsBack_Allowed_NoPrompt) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, FallsBack_Allowed_WithPrompt) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/true);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, FallsBack_Blocked) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, FallsBack_Blocked_WithPrompt) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/true,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/true);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

class TestGatingContext : public GatingDecisionContext {
 public:
  explicit TestGatingContext(int val) : value(val) {}
  ~TestGatingContext() override = default;
  int value;
};

TEST_F(OriginGatingCheckerTest, PlumbsContext) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  auto context = std::make_unique<TestGatingContext>(42);
  GatingDecisionContext* expected_context_ptr = context.get();

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             expected_context_ptr, _, source, destination, _))
      .WillOnce(base::test::RunOnceCallback<4>(true));

  EXPECT_CALL(delegate_,
              OnNoVerdict(expected_context_ptr, _, source, destination,
                          /*requires_user_confirmation=*/true, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = false}));

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(std::move(context),
                                GateableEvent::kNavigationResponse, source,
                                destination, future.GetCallback());

  auto [returned_context, decision] = future.Take();
  EXPECT_THAT(returned_context, Pointer(expected_context_ptr));
  EXPECT_TRUE(decision.is_allowed);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowSameOrigin_ShortCircuits) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kAllowSameOrigin,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com/page1");
  GURL destination("https://example.com/page2");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kAllowSameOrigin);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowSameOrigin_NoDecision_FallsBack) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kAllowSameOrigin,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_Blocked) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration({{DecisionSource::kForbidNonLocalhostIpAddress,
                                  GateableEventSet::All()}},
                                /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://192.168.1.1/page");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kForbidNonLocalhostIpAddress);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_Ipv4LocalhostFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration({{DecisionSource::kForbidNonLocalhostIpAddress,
                                  GateableEventSet::All()}},
                                /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://127.0.0.1/page");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_Ipv6LocalhostFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration({{DecisionSource::kForbidNonLocalhostIpAddress,
                                  GateableEventSet::All()}},
                                /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://[::1]/page");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ForbidNonLocalhostIpAddress_NoDecision_FallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration({{DecisionSource::kForbidNonLocalhostIpAddress,
                                  GateableEventSet::All()}},
                                /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_HttpsFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_HttpBlocked) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kRequireHttpsOrLocalhost);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_DomainLocalhostFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://localhost/page");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_Ipv4LocalhostFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://127.0.0.1/page");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_Ipv6LocalhostFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://[::1]/page");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrLocalhost_NonHttpLocalhostBlocked) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("file://localhost/tmp");

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kRequireHttpsOrLocalhost);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrHttp_HttpsFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrHttp, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrHttp_HttpFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrHttp, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_RequireHttpsOrHttp_NonWebSchemeBlocked) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kRequireHttpsOrHttp, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("file:///tmp/file");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kRequireHttpsOrHttp);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowHttpLocalhost_HttpLocalhostAllowed) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kAllowHttpLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://localhost/path");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kAllowHttpLocalhost);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowHttpLocalhost_NonHttpSchemeFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kAllowHttpLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("ws://localhost/path");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowHttpLocalhost_NonLocalhostFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kAllowHttpLocalhost, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("http://foo.com/path");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowAboutBlank_AboutBlankAllowed) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kAllowAboutBlank,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("about:blank");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kAllowAboutBlank);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_AllowAboutBlank_NonBlankFallsBack) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kAllowAboutBlank,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ActorContainerConfig_NoConfigFallsBack) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kActorContainerConfig, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ActorContainerConfig_NavigationAllowed) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kActorContainerConfig, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  // Configure the slot on the checker.
  checker.actor_container_config_slot().Assign(ActorContainerConfig({{
      {ActorContainerConfig::Location(ActorContainerConfig::Wildcard()),
       ActorContainerConfig::Rule(
           /*navigation_sources=*/{},
           /*resources=*/{ActorContainerConfig::Rule::Resource::kSession},
           /*capabilities=*/{ActorContainerConfig::Rule::Capability::kAll})},
  }}));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  // Since it is explicitly allowed, we don't query the delegate for user
  // confirmation or no-verdict (it does not fall back).
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kActorContainerConfig);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ActorContainerConfig_NavigationBlocked) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kActorContainerConfig, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  // Set an empty config which blocks all navigations.
  checker.actor_container_config_slot().Assign(ActorContainerConfig());

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kActorContainerConfig);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ActorContainerConfig_PageActionAllowed) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kActorContainerConfig, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  // Configure the slot to allow actuation.
  checker.actor_container_config_slot().Assign(ActorContainerConfig({{
      {ActorContainerConfig::Location(ActorContainerConfig::Wildcard()),
       ActorContainerConfig::Rule(
           /*navigation_sources=*/{},
           /*resources=*/{ActorContainerConfig::Rule::Resource::kSession},
           /*capabilities=*/{ActorContainerConfig::Rule::Capability::kAll})},
  }}));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  // Use page action event.
  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(nullptr, GateableEvent::kPageAction, source,
                                destination, future.GetCallback());

  GatingDecision decision = future.Get<1>();
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kActorContainerConfig);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_ActorContainerConfig_PageActionBlocked) {
  OriginGatingChecker checker(
      delegate_,
      OriginGatingConfiguration(
          {{DecisionSource::kActorContainerConfig, GateableEventSet::All()}},
          /*use_site_keyed_cache=*/false));

  // Set an empty config which blocks all actuations.
  checker.actor_container_config_slot().Assign(ActorContainerConfig());

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(nullptr, GateableEvent::kPageAction, source,
                                destination, future.GetCallback());

  GatingDecision decision = future.Get<1>();
  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kActorContainerConfig);
}

TEST_F(OriginGatingCheckerTest, BuiltInPredicate_EnterprisePolicy_Allowed) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kEnterprisePolicy,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin source_origin = url::Origin::Create(source);
  url::Origin destination_origin = url::Origin::Create(destination);

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(DecisionWithMetadata{
          .decision = Decision::kAllowed, .bypass_cache = true}));
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kEnterprisePolicy);
  // With bypass_cache set, the allow decision must not be persisted.
  EXPECT_FALSE(
      checker.cache().IsNavigationAllowed(source_origin, destination_origin));
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_EnterprisePolicy_Allowed_PersistsCache) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kEnterprisePolicy,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin source_origin = url::Origin::Create(source);
  url::Origin destination_origin = url::Origin::Create(destination);

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(DecisionWithMetadata{
          .decision = Decision::kAllowed, .bypass_cache = false}));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kEnterprisePolicy);
  // Without bypass_cache, the allow decision must be persisted.
  EXPECT_TRUE(
      checker.cache().IsNavigationAllowed(source_origin, destination_origin));
}

TEST_F(OriginGatingCheckerTest, BuiltInPredicate_EnterprisePolicy_Blocked) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kEnterprisePolicy,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          DecisionWithMetadata{.decision = Decision::kBlocked}));
  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kEnterprisePolicy);
}

TEST_F(OriginGatingCheckerTest,
       BuiltInPredicate_EnterprisePolicy_NoDecision_FallsBack) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{DecisionSource::kEnterprisePolicy,
                                             GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, EvaluateEnterprisePolicy(destination, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          DecisionWithMetadata{.decision = Decision::kNoDecision}));
  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, AsyncCustomPredicate_Allowed_ShortCircuits) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        EXPECT_EQ(source, GURL("https://example.com"));
        EXPECT_EQ(destination, GURL("https://foo.com"));
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom1);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{custom, GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, TestCustomPredicate::kCustom1);
}

TEST_F(OriginGatingCheckerTest,
       CustomPredicate_AttributionDoesNotMatchDifferentEnumTypeWithSameValue) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GURL&, const GURL&) {
        return Decision::kAllowed;
      }),
      TestCustomPredicate::kCustom1);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{custom, GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, GURL("https://example.com"), GURL("https://foo.com"));

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, TestCustomPredicate::kCustom1);
  EXPECT_EQ(decision.attribution.CustomPredicateId<TestCustomPredicate>(),
            TestCustomPredicate::kCustom1);

  EXPECT_NE(decision.attribution, OtherTestCustomPredicate::kOtherCustom1);
  EXPECT_CHECK_DEATH(
      decision.attribution.CustomPredicateId<OtherTestCustomPredicate>());
}

TEST_F(OriginGatingCheckerTest,
       AsyncCustomPredicate_NoDecision_FallsBackToDelegate) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kNoDecision);
      }),
      TestCustomPredicate::kCustom1);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{custom, GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, SyncCustomPredicate_Allowed_ShortCircuits) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GURL& source,
                             const GURL& destination) {
        EXPECT_EQ(source, GURL("https://example.com"));
        EXPECT_EQ(destination, GURL("https://foo.com"));
        return Decision::kAllowed;
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{custom, GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, TestCustomPredicate::kCustom2);
}

TEST_F(OriginGatingCheckerTest,
       SyncCustomPredicate_NoDecision_FallsBackToDelegate) {
  CustomPredicate custom(
      base::BindRepeating([](GatingDecisionContext*, const GURL&, const GURL&) {
        return Decision::kNoDecision;
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({{custom, GateableEventSet::All()}},
                                           /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       CacheHit_UserConfirmedOrigin_ShortCircuitsImmediately) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration(
                     {
                         {DecisionSource::kCacheWithUserConfirmation,
                          GateableEventSet::All()},
                     },
                     /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker.AllowNavigationTo(destination_origin, /*is_user_confirmed=*/true);

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kCacheWithUserConfirmation);
}

TEST_F(OriginGatingCheckerTest,
       CacheMiss_NonConfirmedOrigin_SensitiveDestination_QueriesDelegate) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration(
                     {
                         {DecisionSource::kCacheWithUserConfirmation,
                          GateableEventSet::All()},
                     },
                     /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker.AllowNavigationTo(destination_origin, /*is_user_confirmed=*/false);

  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, _, source, destination, _))
      .WillOnce(
          base::test::RunOnceCallback<4>(/*requires_user_confirmation=*/true));

  EXPECT_CALL(delegate_, OnNoVerdict(_, _, source, destination,
                                     /*requires_user_confirmation=*/true, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = true}));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest,
       CacheHit_NonConfirmedOrigin_NonSensitiveDestination_ShortCircuits) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration(
                     {
                         {DecisionSource::kCacheWithUserConfirmation,
                          GateableEventSet::All()},
                         {DecisionSource::kCacheWithoutUserConfirmation,
                          GateableEventSet::All()},
                     },
                     /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin destination_origin = url::Origin::Create(destination);

  checker.AllowNavigationTo(destination_origin, /*is_user_confirmed=*/false);

  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, _, source, destination, _))
      .WillOnce(
          base::test::RunOnceCallback<4>(/*requires_user_confirmation=*/false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution,
            DecisionSource::kCacheWithoutUserConfirmation);
}

TEST_F(OriginGatingCheckerTest, PredicateSkipped_WhenEventNotApplicable) {
  // A custom predicate that would allow, but is restricted to kPageAction only.
  CustomPredicate page_action_only(
      base::BindRepeating([](GatingDecisionContext*, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration(
                     {
                         {page_action_only, {GateableEvent::kPageAction}},
                     },
                     /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  // For a navigation-request event the predicate is skipped, so the checker
  // falls back to the delegate.
  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, _, source, destination, _))
      .WillOnce(base::test::RunOnceCallback<4>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, source, destination, false, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = false, .did_prompt_user = false}));

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(nullptr, GateableEvent::kNavigationRequest,
                                source, destination, future.GetCallback());
  GatingDecision decision = future.Get<1>();
  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, PredicateRuns_WhenEventApplicable) {
  CustomPredicate page_action_only(
      base::BindRepeating([](GatingDecisionContext*, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration(
                     {
                         {page_action_only, {GateableEvent::kPageAction}},
                     },
                     /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, _, _, _, _)).Times(0);

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(nullptr, GateableEvent::kPageAction, source,
                                destination, future.GetCallback());
  GatingDecision decision = future.Get<1>();
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, TestCustomPredicate::kCustom2);
}

TEST_F(OriginGatingCheckerTest, EventReachesPredicateAndDelegate) {
  // The custom predicate returns kNoDecision so evaluation reaches the
  // delegate, allowing us to assert the event is threaded through both hops.
  CustomPredicate observing_predicate(
      base::BindRepeating([](GatingDecisionContext*, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kNoDecision);
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration(
                     {{observing_predicate, GateableEventSet::All()}},
                     /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, GateableEvent::kPageAction,
                                                source, destination, _))
      .WillOnce(base::test::RunOnceCallback<4>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, GateableEvent::kPageAction, source,
                                     destination, false, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = false}));

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(nullptr, GateableEvent::kPageAction, source,
                                destination, future.GetCallback());
  GatingDecision decision = future.Get<1>();
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, BypassCache_SuppressesCacheWrite) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin source_origin = url::Origin::Create(source);
  url::Origin destination_origin = url::Origin::Create(destination);

  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, _, source, destination, _))
      .WillOnce(base::test::RunOnceCallback<4>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, source, destination, false, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true,
              .did_prompt_user = false,
              .bypass_cache = true}));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  // The allow decision must not have been persisted.
  EXPECT_FALSE(
      checker.cache().IsNavigationAllowed(source_origin, destination_origin));
}

TEST_F(OriginGatingCheckerTest, NoBypassCache_PersistsCacheWrite) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin source_origin = url::Origin::Create(source);
  url::Origin destination_origin = url::Origin::Create(destination);

  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, _, source, destination, _))
      .WillOnce(base::test::RunOnceCallback<4>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, source, destination, false, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true,
              .did_prompt_user = false,
              .bypass_cache = false}));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  // The allow decision must have been persisted.
  EXPECT_TRUE(
      checker.cache().IsNavigationAllowed(source_origin, destination_origin));
}

TEST_F(OriginGatingCheckerTest, BypassCache_IgnoredWhenBlocked) {
  OriginGatingChecker checker(
      delegate_, OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));

  GURL source("https://example.com");
  GURL destination("https://foo.com");
  url::Origin source_origin = url::Origin::Create(source);
  url::Origin destination_origin = url::Origin::Create(destination);

  EXPECT_CALL(delegate_,
              DoesOriginRequireUserConfirmation(_, _, source, destination, _))
      .WillOnce(base::test::RunOnceCallback<4>(false));
  EXPECT_CALL(delegate_, OnNoVerdict(_, _, source, destination, false, _))
      .WillOnce(base::test::RunOnceCallback<5>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = false,
              .did_prompt_user = false,
              .bypass_cache = false}));

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  // A blocked decision is never persisted regardless of bypass_cache.
  EXPECT_FALSE(
      checker.cache().IsNavigationAllowed(source_origin, destination_origin));
}

}  // namespace
}  // namespace origin_gating
