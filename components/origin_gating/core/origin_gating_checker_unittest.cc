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
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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

TEST_F(OriginGatingCheckerTest, CustomPredicate_Allowed_ShortCircuits) {
  CustomPredicate custom(
      base::BindRepeating([](const GatingDecisionContext* context,
                             GateableEvent event, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        EXPECT_EQ(source, GURL("https://example.com"));
        EXPECT_EQ(destination, GURL("https://foo.com"));
        std::move(callback).Run(Decision::kAllowed);
      }),
      "my_custom_predicate");

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
  EXPECT_EQ(decision.attribution, "my_custom_predicate");
}

TEST_F(OriginGatingCheckerTest,
       CustomPredicate_NoDecision_FallsBackToDelegate) {
  CustomPredicate custom(
      base::BindRepeating([](const GatingDecisionContext* context,
                             GateableEvent event, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kNoDecision);
      }),
      "my_custom_predicate");

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
      base::BindRepeating([](const GatingDecisionContext* context,
                             GateableEvent event, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      "page_action_only");

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
      base::BindRepeating([](const GatingDecisionContext* context,
                             GateableEvent event, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      "page_action_only");

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
  EXPECT_EQ(decision.attribution, "page_action_only");
}

TEST_F(OriginGatingCheckerTest, EventReachesPredicateAndDelegate) {
  // The custom predicate returns kNoDecision so evaluation reaches the
  // delegate, allowing us to assert the event is threaded through both hops.
  CustomPredicate observing_predicate(
      base::BindRepeating([](const GatingDecisionContext* context,
                             GateableEvent event, const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
        EXPECT_EQ(event, GateableEvent::kPageAction);
        std::move(callback).Run(Decision::kNoDecision);
      }),
      "observing_predicate");

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

}  // namespace
}  // namespace origin_gating
