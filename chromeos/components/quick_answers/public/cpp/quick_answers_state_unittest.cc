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
#include "chromeos/components/magic_boost/test/fake_magic_boost_state.h"
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
  void OnConsentStatusUpdated(
      quick_answers::prefs::ConsentStatus consent_status) override {
    consent_status_ = consent_status;
  }

  bool is_eligible() const { return is_eligible_; }
  bool is_enabled() const { return is_enabled_; }
  quick_answers::prefs::ConsentStatus consent_status() const {
    return consent_status_;
  }

 private:
  bool is_eligible_ = false;
  bool is_enabled_ = false;
  quick_answers::prefs::ConsentStatus consent_status_ =
      quick_answers::prefs::ConsentStatus::kUnknown;
};

std::unique_ptr<base::test::ScopedFeatureList> MaybeEnableMagicBoost() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Note that `kMahi` is associated with the Magic Boost feature.
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitWithFeatures(
      {chromeos::features::kMahi, chromeos::features::kFeatureManagementMahi},
      {});
  return feature_list;
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

  chromeos::test::FakeMagicBoostState magic_boost_state;
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
  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);

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
  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);

  EXPECT_FALSE(QuickAnswersState::IsEnabled());
}

TEST(QuickAnswersStateTest, IsEnabledObserverInit) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);
  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);

  FakeObserver observer;
  quick_answers_state.AddObserver(&observer);
  EXPECT_TRUE(observer.is_enabled());
}

TEST(QuickAnswersStateTest, IsEnabledUnderMagicBoost) {
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  chromeos::test::FakeMagicBoostState magic_boost_state;
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

  magic_boost_state.AsyncWriteHMREnabled(true);
  magic_boost_state.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kApproved);

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
  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);

  EXPECT_FALSE(
      QuickAnswersState::IsEnabledAs(QuickAnswersState::FeatureType::kHmr));
}

TEST(QuickAnswersStateTest, IsEnabledGatedByConsentStatus) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);
  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);
  EXPECT_TRUE(QuickAnswersState::IsEnabled());

  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kRejected);
  EXPECT_FALSE(QuickAnswersState::IsEnabled())
      << "IsEnabled requires kAccepted consent status";
}

TEST(QuickAnswersStateTest, GetConsentStatus) {
  EXPECT_EQ(base::unexpected(QuickAnswersState::Error::kUninitialized),
            QuickAnswersState::GetConsentStatus());

  FakeObserver observer;
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.AddObserver(&observer);
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetSettingsEnabled(true);

  EXPECT_EQ(base::unexpected(QuickAnswersState::Error::kUninitialized),
            QuickAnswersState::GetConsentStatus());

  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kUnknown);
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            QuickAnswersState::GetConsentStatus());
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            observer.consent_status());

  quick_answers_state.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kAccepted,
            QuickAnswersState::GetConsentStatus());
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kAccepted,
            observer.consent_status());
}

TEST(QuickAnswersStateTest, GetConsentStatusUnderMagicBoost) {
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  chromeos::test::FakeMagicBoostState magic_boost_state;
  FakeObserver observer;
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.AddObserver(&observer);
  quick_answers_state.SetApplicationLocale("en");

  EXPECT_EQ(base::unexpected(QuickAnswersState::Error::kUninitialized),
            QuickAnswersState::GetConsentStatus());

  magic_boost_state.AsyncWriteConsentStatus(chromeos::HMRConsentStatus::kUnset);
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            QuickAnswersState::GetConsentStatus());
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            observer.consent_status());

  magic_boost_state.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kApproved);
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kAccepted,
            QuickAnswersState::GetConsentStatus());
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kAccepted,
            observer.consent_status());
}

TEST(QuickAnswersStateTest, PendingUnderMagicBoost) {
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  chromeos::test::FakeMagicBoostState magic_boost_state;
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  magic_boost_state.AsyncWriteHMREnabled(true);
  magic_boost_state.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kPendingDisclaimer);

  EXPECT_TRUE(
      quick_answers_state.IsEnabledAs(QuickAnswersState::FeatureType::kHmr))
      << "Quick Answers capability can be enabled with kPendingDisclaimer "
         "state.";
  EXPECT_FALSE(quick_answers_state.IsEnabledAs(
      QuickAnswersState::FeatureType::kQuickAnswers))
      << "Expect that Quick Answers capability is enabled only as HMR if it's "
         "via MagicBoost.";
}

TEST(QuickAnswersStateTest, PendingNotEnabledUnderMagicBoost) {
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  chromeos::test::FakeMagicBoostState magic_boost_state;
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  magic_boost_state.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kPendingDisclaimer);

  ASSERT_FALSE(magic_boost_state.hmr_enabled().has_value())
      << "Test that enabled state pref value is not initialized yet.";
  EXPECT_FALSE(
      quick_answers_state.IsEnabledAs(QuickAnswersState::FeatureType::kHmr))
      << "Magic Boost must be enabled as Quick Answers capability can be "
         "enabled.";

  magic_boost_state.AsyncWriteHMREnabled(false);
  ASSERT_TRUE(magic_boost_state.hmr_enabled().has_value());
  ASSERT_FALSE(magic_boost_state.hmr_enabled().value())
      << "This test is testing kPendingDisclaimer state with not-enabled";

  EXPECT_FALSE(
      quick_answers_state.IsEnabledAs(QuickAnswersState::FeatureType::kHmr))
      << "Magic Boost must be enabled as Quick Answers capability can be "
         "enabled.";
}

TEST(QuickAnswersStateTest, IsIntentEligible) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kDefinition))
      << "Expect false if a respective intent eligible value is not "
         "initialized";
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kTranslation))
      << "Expect false if a respective intent eligible value is not "
         "initialized";
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kUnitConversion))
      << "Expect false if a respective intent eligible value is not "
         "initialized";

  // Enables intent one by one and confirm that it's reflected.
  quick_answers_state.SetDefinitionEligible(true);
  EXPECT_TRUE(QuickAnswersState::IsIntentEligible(Intent::kDefinition));
  quick_answers_state.SetTranslationEligible(true);
  EXPECT_TRUE(QuickAnswersState::IsIntentEligible(Intent::kTranslation));
  quick_answers_state.SetUnitConversionEligible(true);
  EXPECT_TRUE(QuickAnswersState::IsIntentEligible(Intent::kUnitConversion));

  // Disables intent one by one and confirm that it's reflected.
  quick_answers_state.SetDefinitionEligible(false);
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kDefinition));
  quick_answers_state.SetTranslationEligible(false);
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kTranslation));
  quick_answers_state.SetUnitConversionEligible(false);
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kUnitConversion));
}

TEST(QuickAnswersStateTest, IsIntentEligibleGatedByGlobalEligible) {
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  quick_answers_state.SetDefinitionEligible(true);
  quick_answers_state.SetTranslationEligible(true);
  quick_answers_state.SetUnitConversionEligible(true);
  ASSERT_TRUE(QuickAnswersState::IsEligible());
  ASSERT_TRUE(QuickAnswersState::IsIntentEligible(Intent::kDefinition))
      << "Precondition: intent is eligible";
  ASSERT_TRUE(QuickAnswersState::IsIntentEligible(Intent::kTranslation))
      << "Precondition: intent is eligible";
  ASSERT_TRUE(QuickAnswersState::IsIntentEligible(Intent::kUnitConversion))
      << "Precondition: intent is eligible";

  quick_answers_state.SetSettingsEnabled(false);
  quick_answers_state.SetApplicationLocale("ja");
  ASSERT_FALSE(QuickAnswersState::IsEligible())
      << "Precondition: feature itself is ineligible";
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kDefinition))
      << "Intent is ineligible if a feature itself is ineligible";
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kTranslation))
      << "Intent is ineligible if a feature itself is ineligible";
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(Intent::kUnitConversion))
      << "Intent is ineligible if a feature itself is ineligible";
}

TEST(QuickAnswersStateTest, IsIntentEligibleUnderMagicBoost) {
  std::unique_ptr<base::test::ScopedFeatureList> magic_boost_enabled =
      MaybeEnableMagicBoost();
  ASSERT_TRUE(magic_boost_enabled)
      << "This *test* code does not support Lacros. See MaybeEnableMagicBoost.";

  chromeos::test::FakeMagicBoostState magic_boost_state;

  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");

  quick_answers_state.SetDefinitionEligible(false);
  quick_answers_state.SetTranslationEligible(false);
  quick_answers_state.SetUnitConversionEligible(false);
  ASSERT_FALSE(QuickAnswersState::IsIntentEligibleAs(
      Intent::kDefinition, QuickAnswersState::FeatureType::kQuickAnswers))
      << "Precondition: intent is ineligible for Quick Answers";
  ASSERT_FALSE(QuickAnswersState::IsIntentEligibleAs(
      Intent::kTranslation, QuickAnswersState::FeatureType::kQuickAnswers))
      << "Precondition: intent is ineligible for Quick Answers";
  ASSERT_FALSE(QuickAnswersState::IsIntentEligibleAs(
      Intent::kUnitConversion, QuickAnswersState::FeatureType::kQuickAnswers))
      << "Precondition: intent is ineligible for Quick Answers";

  EXPECT_TRUE(QuickAnswersState::IsIntentEligibleAs(
      Intent::kDefinition, QuickAnswersState::FeatureType::kHmr))
      << "An intent is always eligible for kHmr regardless the respective "
         "value in kQuickAnswers";
  EXPECT_TRUE(QuickAnswersState::IsIntentEligibleAs(
      Intent::kTranslation, QuickAnswersState::FeatureType::kHmr))
      << "An intent is always eligible for kHmr regardless the respective "
         "value in kQuickAnswers";
  EXPECT_TRUE(QuickAnswersState::IsIntentEligibleAs(
      Intent::kUnitConversion, QuickAnswersState::FeatureType::kHmr))
      << "An intent is always eligible for kHmr regardless the respective "
         "value in kQuickAnswers";
}

}  // namespace quick_answers
