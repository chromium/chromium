// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/scalable_iph/mock_scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scalable_iph {
namespace {

struct TransitionExpectation {
  ScalableIphDelegate::SessionState from;
  ScalableIphDelegate::SessionState to;
  ScalableIph::TransitionSet transition;
};

constexpr char kTestTriggerEventParamName[] =
    "IPH_ScalableIphUnlockedBasedOne_x_CustomConditionTriggerEvent";
const base::Feature& kTestFeature =
    feature_engagement::kIPHScalableIphUnlockedBasedOneFeature;

}  // namespace

// Test all 4*4 = 16 session state transitions.
TEST(ScalableIphTest, SessionStateTransitionTest) {
  std::vector<TransitionExpectation> expectations;

  // From `kUnknownInitialValue`:
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .to = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .transition = {}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .to = ScalableIphDelegate::SessionState::kActive,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState,
                      ScalableIph::SessionStateTransition::kUnlock}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .to = ScalableIphDelegate::SessionState::kLocked,
       .transition = {}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .to = ScalableIphDelegate::SessionState::kOther,
       .transition = {}});

  // From `kActive`:
  // There should be no to=kUnknownInitialValue transition.
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kActive,
       .to = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .transition = {}});
  expectations.push_back({.from = ScalableIphDelegate::SessionState::kActive,
                          .to = ScalableIphDelegate::SessionState::kActive,
                          .transition = {}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kActive,
       .to = ScalableIphDelegate::SessionState::kLocked,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kActive,
       .to = ScalableIphDelegate::SessionState::kOther,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState}});

  // From `kLocked`:
  // There should be no to=kUnknownInitialValue transition.
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kLocked,
       .to = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .transition = {}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kLocked,
       .to = ScalableIphDelegate::SessionState::kActive,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState,
                      ScalableIph::SessionStateTransition::kUnlock}});
  expectations.push_back({.from = ScalableIphDelegate::SessionState::kLocked,
                          .to = ScalableIphDelegate::SessionState::kLocked,
                          .transition = {}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kLocked,
       .to = ScalableIphDelegate::SessionState::kOther,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState}});

  // From `kOther`:
  // There should be no to=kUnknownInitialValue transition.
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kOther,
       .to = ScalableIphDelegate::SessionState::kUnknownInitialValue,
       .transition = {}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kOther,
       .to = ScalableIphDelegate::SessionState::kActive,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState}});
  expectations.push_back(
      {.from = ScalableIphDelegate::SessionState::kOther,
       .to = ScalableIphDelegate::SessionState::kLocked,
       .transition = {ScalableIph::SessionStateTransition::kAdvanceState}});
  expectations.push_back({.from = ScalableIphDelegate::SessionState::kOther,
                          .to = ScalableIphDelegate::SessionState::kOther,
                          .transition = {}});

  for (TransitionExpectation expectation : expectations) {
    ScalableIph::TransitionSet expected = expectation.transition;
    ScalableIph::TransitionSet actual =
        ScalableIph::GetTransitionForTesting(expectation.from, expectation.to);

    EXPECT_EQ(expected, actual)
        << "Expectation failed for transition from " << expectation.from
        << " to " << expectation.to << ". Expected transition is "
        << expected.ToString() << ". Actual transition is "
        << actual.ToString();

    if (actual.Has(ScalableIph::SessionStateTransition::kUnlock)) {
      EXPECT_TRUE(
          actual.Has(ScalableIph::SessionStateTransition::kAdvanceState))
          << "kAdvanceState is a necessary condition of kUnlock";
      EXPECT_EQ(expectation.to, ScalableIphDelegate::SessionState::kActive)
          << "kActive is a necessary condition of kUnlock";
    }
  }
}

TEST(ScalableIphTest, TriggerEventCondition) {
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params(
      {{kTestTriggerEventParamName, kEventNameUnlocked}});
  scoped_feature_list.InitAndEnableFeatureWithParameters(kTestFeature, params);

  feature_engagement::test::MockTracker mock_tracker;

  ScalableIph scalable_iph(
      &mock_tracker, std::make_unique<ash::test::MockScalableIphDelegate>(),
      std::make_unique<Logger>());

  EXPECT_FALSE(scalable_iph.CheckTriggerEventForTesting(
      kTestFeature, /*trigger_event=*/std::nullopt))
      << "If condition specified, the condition only satisfied if this check "
         "is triggered by the specified event";

  EXPECT_TRUE(scalable_iph.CheckTriggerEventForTesting(
      kTestFeature, ScalableIph::Event::kUnlocked));

  EXPECT_FALSE(scalable_iph.CheckTriggerEventForTesting(
      kTestFeature, ScalableIph::Event::kFiveMinTick));
}

TEST(ScalableIphTest, TriggerEventConditionNotSpecified) {
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::ScopedFeatureList scoped_feature_list(kTestFeature);

  feature_engagement::test::MockTracker mock_tracker;

  ScalableIph scalable_iph(
      &mock_tracker, std::make_unique<ash::test::MockScalableIphDelegate>(),
      std::make_unique<Logger>());

  EXPECT_TRUE(scalable_iph.CheckTriggerEventForTesting(
      kTestFeature, /*trigger_event=*/std::nullopt))
      << "Condition always satisfied if this condition not specified";

  EXPECT_TRUE(scalable_iph.CheckTriggerEventForTesting(
      kTestFeature, ScalableIph::Event::kUnlocked))
      << "Condition always satisfied if this condition not specified";
}

}  // namespace scalable_iph
