// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scalable_iph {
namespace {
struct TransitionExpectation {
  ScalableIphDelegate::SessionState from;
  ScalableIphDelegate::SessionState to;
  ScalableIph::TransitionSet transition;
};
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

}  // namespace scalable_iph
