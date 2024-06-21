// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

class FakeObserver : public QuickAnswersStateObserver {
 public:
  void OnEligibilityChanged(bool eligible) override { is_eligible_ = eligible; }

  bool is_eligible() const { return is_eligible_; }

 private:
  bool is_eligible_ = false;
};

}  // namespace

TEST(QuickAnswersStateTest, IsEligible) {
  FakeQuickAnswersState quick_answers_state;
  FakeObserver observer;
  quick_answers_state.AddObserver(&observer);
  EXPECT_FALSE(QuickAnswersState::IsEligible());

  quick_answers_state.SetApplicationLocale("en");
  EXPECT_TRUE(QuickAnswersState::IsEligible());
  EXPECT_TRUE(observer.is_eligible());

  quick_answers_state.SetApplicationLocale("ja");
  EXPECT_FALSE(QuickAnswersState::IsEligible());
  EXPECT_FALSE(observer.is_eligible());

  quick_answers_state.SetApplicationLocale("en-US");
  EXPECT_TRUE(QuickAnswersState::IsEligible());
  EXPECT_TRUE(observer.is_eligible());
}

TEST(QuickAnswersStateTest, IsEligibleObserverInit) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  FakeObserver observer;
  quick_answers_state.AddObserver(&observer);
  EXPECT_TRUE(observer.is_eligible());
}

TEST(QuickAnswersStateTest, IsEligibleFeatureType) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kMagicBoost);

  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  EXPECT_FALSE(QuickAnswersState::IsEligibleAs(
      QuickAnswersState::FeatureType::kQuickAnswers));
  EXPECT_TRUE(
      QuickAnswersState::IsEligibleAs(QuickAnswersState::FeatureType::kHmr));
}
}  // namespace quick_answers
