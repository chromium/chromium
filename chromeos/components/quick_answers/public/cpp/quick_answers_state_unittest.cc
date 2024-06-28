// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

class FakeObserver : public QuickAnswersStateObserver {
 public:
  void OnEligibilityChanged(bool eligible) override { is_eligible_ = eligible; }
  void OnSettingsEnabled(bool enabled) override { is_enabled_ = enabled; }

  bool is_eligible() const { return is_eligible_; }
  bool is_enabled() const { return is_enabled_; }

 private:
  bool is_eligible_ = false;
  bool is_enabled_ = false;
};

class FakeMagicBoostState : public chromeos::MagicBoostState {
 public:
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override { return 0; }
  void AsyncWriteConsentStatus(chromeos::HMRConsentStatus) override {}
  void AsyncWriteHMREnabled(bool) override {}
  void ShouldIncludeOrcaInOptIn(base::OnceCallback<void(bool)>) override {}
  void DisableOrcaFeature() override {}

  void SetHMREnabledForTesting(bool enabled) { UpdateHMREnabled(enabled); }
};

std::unique_ptr<base::test::ScopedFeatureList> MaybeEnableMagicBoost() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<base::test::ScopedFeatureList>(
      chromeos::features::kMagicBoost);
#else
  // chromeos_components_unittests is expected to run only in Ash build for now.
  //
  // Build config:
  // https://source.chromium.org/chromium/chromium/src/+/main:BUILD.gn;l=454;drc=bfcb02f1ceb574659c9f0d9b5eb1cbf85040696b
  return nullptr;
#endif
}

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
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  FakeMagicBoostState magic_boost_state;
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  EXPECT_FALSE(QuickAnswersState::IsEligibleAs(
      QuickAnswersState::FeatureType::kQuickAnswers));
  EXPECT_TRUE(
      QuickAnswersState::IsEligibleAs(QuickAnswersState::FeatureType::kHmr));
}

TEST(QuickAnswersStateTest, IsEnabled) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  FakeObserver observer;
  quick_answers_state.AddObserver(&observer);
  EXPECT_FALSE(QuickAnswersState::IsEnabled());

  quick_answers_state.SetSettingsEnabled(true);
  EXPECT_TRUE(QuickAnswersState::IsEnabled());
  EXPECT_TRUE(observer.is_enabled());

  quick_answers_state.SetSettingsEnabled(false);
  EXPECT_FALSE(QuickAnswersState::IsEnabled());
  EXPECT_FALSE(observer.is_enabled());
}

TEST(QuickAnswersStateTest, EnabledButNotEligible) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("ja");
  quick_answers_state.SetSettingsEnabled(true);

  EXPECT_FALSE(QuickAnswersState::IsEnabled());
}

TEST(QuickAnswersStateTest, EnabledButKiosk) {
  user_manager::ScopedUserManager scoped_user_manager(
      std::make_unique<user_manager::FakeUserManager>());
  chromeos::SetUpFakeKioskSession();

  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);

  EXPECT_FALSE(QuickAnswersState::IsEnabled());
}

TEST(QuickAnswersStateTest, IsEnabledObserverInit) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);

  FakeObserver observer;
  quick_answers_state.AddObserver(&observer);
  EXPECT_TRUE(observer.is_enabled());
}

TEST(QuickAnswersStateTest, IsEnabledUnderMagicBoost) {
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  FakeMagicBoostState magic_boost_state;
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);
  EXPECT_FALSE(QuickAnswersState::IsEnabledAs(
      QuickAnswersState::FeatureType::kQuickAnswers));

  FakeObserver observer;
  quick_answers_state.AddObserver(&observer);

  EXPECT_FALSE(
      QuickAnswersState::IsEnabledAs(QuickAnswersState::FeatureType::kHmr));
  EXPECT_FALSE(observer.is_enabled());

  magic_boost_state.SetHMREnabledForTesting(true);

  EXPECT_TRUE(
      QuickAnswersState::IsEnabledAs(QuickAnswersState::FeatureType::kHmr));
  EXPECT_TRUE(observer.is_enabled());
}

TEST(QuickAnswersStateTest, IsEnabledAsMagicBoostUnderQuickAnswers) {
  ASSERT_EQ(QuickAnswersState::FeatureType::kQuickAnswers,
            QuickAnswersState::GetFeatureType());

  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);
  EXPECT_FALSE(
      QuickAnswersState::IsEnabledAs(QuickAnswersState::FeatureType::kHmr));
}

}  // namespace quick_answers
