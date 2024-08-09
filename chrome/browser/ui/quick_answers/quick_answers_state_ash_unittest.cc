// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_state_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace {

using quick_answers::prefs::ConsentStatus;

constexpr char kTestUser[] = "user@gmail.com";

}  // namespace

class TestQuickAnswersStateObserver : public QuickAnswersStateObserver {
 public:
  TestQuickAnswersStateObserver() = default;

  TestQuickAnswersStateObserver(const TestQuickAnswersStateObserver&) = delete;
  TestQuickAnswersStateObserver& operator=(
      const TestQuickAnswersStateObserver&) = delete;

  ~TestQuickAnswersStateObserver() override = default;

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool settings_enabled) override {
    settings_enabled_ = settings_enabled;
  }
  void OnConsentStatusUpdated(ConsentStatus status) override {
    consent_status_ = status;
  }
  void OnApplicationLocaleReady(
      const std::string& application_locale) override {
    application_locale_ = application_locale;
  }
  void OnPreferredLanguagesChanged(
      const std::string& preferred_languages) override {
    preferred_languages_ = preferred_languages;
  }
  void OnEligibilityChanged(bool eligible) override { is_eligible_ = eligible; }
  void OnPrefsInitialized() override { prefs_initialized_ = true; }

  bool settings_enabled() const { return settings_enabled_; }
  ConsentStatus consent_status() const { return consent_status_; }
  const std::string& application_locale() const { return application_locale_; }
  const std::string& preferred_languages() const {
    return preferred_languages_;
  }
  bool is_eligible() const { return is_eligible_; }
  bool prefs_initialized() const { return prefs_initialized_; }

 private:
  bool settings_enabled_ = false;
  ConsentStatus consent_status_ = ConsentStatus::kUnknown;
  std::string application_locale_;
  std::string preferred_languages_;
  bool is_eligible_ = false;
  bool prefs_initialized_ = false;
};

// The bool parameter controls the feature flag QuickAnswers.
class QuickAnswersStateAshTest : public ChromeQuickAnswersTestBase,
                                 public testing::WithParamInterface<bool> {
 protected:
  QuickAnswersStateAshTest() = default;
  QuickAnswersStateAshTest(const QuickAnswersStateAshTest&) = delete;
  QuickAnswersStateAshTest& operator=(const QuickAnswersStateAshTest&) = delete;
  ~QuickAnswersStateAshTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();
    CHECK(QuickAnswersState::Get()->prefs_initialized());

    observer_ = std::make_unique<TestQuickAnswersStateObserver>();
  }

  void SetUpInitialPrefValues() override {
    prefs_ = static_cast<TestingPrefServiceSimple*>(
        ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService());
    CHECK(prefs_);
  }

  TestingPrefServiceSimple* prefs() { return prefs_; }

  TestQuickAnswersStateObserver* observer() { return observer_.get(); }

 private:
  raw_ptr<TestingPrefServiceSimple, DanglingUntriaged> prefs_ = nullptr;
  std::unique_ptr<TestQuickAnswersStateObserver> observer_;
};

class QuickAnswersStateAshEnabledTest : public QuickAnswersStateAshTest {
 protected:
  void SetUpInitialPrefValues() override {
    QuickAnswersStateAshTest::SetUpInitialPrefValues();

    prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
    CHECK_EQ(
        ConsentStatus::kUnknown,
        prefs()->GetInteger(quick_answers::prefs::kQuickAnswersConsentStatus));
  }
};

TEST_F(QuickAnswersStateAshTest, InitObserver) {
  EXPECT_FALSE(QuickAnswersState::IsEnabled());
  EXPECT_EQ(QuickAnswersState::GetConsentStatus(), ConsentStatus::kUnknown);
  EXPECT_EQ(QuickAnswersState::Get()->application_locale(), std::string());

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  prefs()->SetInteger(quick_answers::prefs::kQuickAnswersConsentStatus,
                      ConsentStatus::kAccepted);
  const std::string application_locale = "en-US";
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);

  EXPECT_TRUE(QuickAnswersState::IsEnabled());
  EXPECT_EQ(QuickAnswersState::GetConsentStatus(), ConsentStatus::kAccepted);
  EXPECT_EQ(QuickAnswersState::Get()->application_locale(), application_locale);

  // The observer class should get an instant notification about the current
  // pref value.
  QuickAnswersState::Get()->AddObserver(observer());
  EXPECT_TRUE(observer()->settings_enabled());
  EXPECT_EQ(observer()->consent_status(), ConsentStatus::kAccepted);
  EXPECT_EQ(observer()->application_locale(), application_locale);
  EXPECT_TRUE(observer()->prefs_initialized());

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, NotifySettingsEnabled) {
  QuickAnswersState::Get()->AddObserver(observer());

  const std::string application_locale = "en-US";
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);

  EXPECT_FALSE(QuickAnswersState::IsEnabled());
  EXPECT_FALSE(observer()->settings_enabled());
  EXPECT_EQ(QuickAnswersState::GetConsentStatus(),
            quick_answers::prefs::ConsentStatus::kUnknown);

  // The observer class should get an notification when the pref value changes.
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  EXPECT_TRUE(QuickAnswersState::IsEnabled());
  EXPECT_TRUE(observer()->settings_enabled());

  // Consent status should also be set to accepted since the feature is
  // explicitly enabled.
  EXPECT_EQ(QuickAnswersState::GetConsentStatus(),
            quick_answers::prefs::ConsentStatus::kAccepted);

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, UpdateConsentStatus) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_EQ(QuickAnswersState::GetConsentStatus(),
            quick_answers::prefs::ConsentStatus::kUnknown);
  EXPECT_EQ(observer()->consent_status(),
            quick_answers::prefs::ConsentStatus::kUnknown);

  // The observer class should get an notification when the pref value changes.
  prefs()->SetInteger(quick_answers::prefs::kQuickAnswersConsentStatus,
                      quick_answers::prefs::ConsentStatus::kRejected);
  EXPECT_EQ(QuickAnswersState::GetConsentStatus(),
            quick_answers::prefs::ConsentStatus::kRejected);
  EXPECT_EQ(observer()->consent_status(),
            quick_answers::prefs::ConsentStatus::kRejected);

  prefs()->SetInteger(quick_answers::prefs::kQuickAnswersConsentStatus,
                      quick_answers::prefs::ConsentStatus::kAccepted);
  EXPECT_EQ(QuickAnswersState::GetConsentStatus(),
            quick_answers::prefs::ConsentStatus::kAccepted);
  EXPECT_EQ(observer()->consent_status(),
            quick_answers::prefs::ConsentStatus::kAccepted);

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, UpdateDefinitionEnabled) {
  const std::string application_locale = "en-US";
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);

  // Definition subfeature is default on.
  EXPECT_TRUE(
      QuickAnswersState::IsIntentEligible(quick_answers::Intent::kDefinition));

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersDefinitionEnabled,
                      false);
  EXPECT_FALSE(
      QuickAnswersState::IsIntentEligible(quick_answers::Intent::kDefinition));
}

TEST_F(QuickAnswersStateAshTest, UpdateTranslationEnabled) {
  const std::string application_locale = "en-US";
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);

  // Translation subfeature is default on.
  EXPECT_TRUE(
      QuickAnswersState::IsIntentEligible(quick_answers::Intent::kTranslation));

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersTranslationEnabled,
                      false);
  EXPECT_FALSE(
      QuickAnswersState::IsIntentEligible(quick_answers::Intent::kTranslation));
}

TEST_F(QuickAnswersStateAshTest, UpdateUnitConversionEnabled) {
  const std::string application_locale = "en-US";
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);

  // Unit conversion subfeature is default on.
  EXPECT_TRUE(QuickAnswersState::IsIntentEligible(
      quick_answers::Intent::kUnitConversion));

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersUnitConversionEnabled,
                      false);
  EXPECT_FALSE(QuickAnswersState::IsIntentEligible(
      quick_answers::Intent::kUnitConversion));
}

TEST_F(QuickAnswersStateAshTest, NotifyApplicationLocaleReady) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_TRUE(QuickAnswersState::Get()->application_locale().empty());
  EXPECT_TRUE(observer()->application_locale().empty());

  const std::string application_locale = "en-US";

  // The observer class should get an notification when the pref value changes.
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);
  EXPECT_EQ(QuickAnswersState::Get()->application_locale(), application_locale);
  EXPECT_EQ(observer()->application_locale(), application_locale);

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, UpdatePreferredLanguages) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_TRUE(QuickAnswersState::Get()->preferred_languages().empty());
  EXPECT_TRUE(observer()->preferred_languages().empty());

  const std::string preferred_languages = "en-US,zh";
  prefs()->SetString(language::prefs::kPreferredLanguages, preferred_languages);
  EXPECT_EQ(QuickAnswersState::Get()->preferred_languages(),
            preferred_languages);
  EXPECT_EQ(observer()->preferred_languages(), preferred_languages);

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, UpdateSpokenFeedbackEnabled) {
  EXPECT_FALSE(QuickAnswersState::Get()->spoken_feedback_enabled());

  prefs()->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  EXPECT_TRUE(QuickAnswersState::Get()->spoken_feedback_enabled());
}

TEST_F(QuickAnswersStateAshTest, EligibleLocales) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::IsEligible());
  EXPECT_FALSE(observer()->is_eligible());

  prefs()->SetString(language::prefs::kApplicationLocale, "pt");
  SimulateUserLogin(kTestUser);
  EXPECT_TRUE(QuickAnswersState::IsEligible());
  EXPECT_TRUE(observer()->is_eligible());

  ClearLogin();

  prefs()->SetString(language::prefs::kApplicationLocale, "en");
  SimulateUserLogin(kTestUser);
  EXPECT_TRUE(QuickAnswersState::IsEligible());
  EXPECT_TRUE(observer()->is_eligible());
}

TEST_F(QuickAnswersStateAshTest, IneligibleLocales) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::IsEligible());
  EXPECT_FALSE(observer()->is_eligible());

  prefs()->SetString(language::prefs::kApplicationLocale, "zh");
  SimulateUserLogin(kTestUser);
  EXPECT_FALSE(QuickAnswersState::IsEligible());
  EXPECT_FALSE(observer()->is_eligible());

  ClearLogin();

  prefs()->SetString(language::prefs::kApplicationLocale, "ja");
  SimulateUserLogin(kTestUser);
  EXPECT_FALSE(QuickAnswersState::IsEligible());
  EXPECT_FALSE(observer()->is_eligible());
}

TEST_F(QuickAnswersStateAshTest, DisabledByPolicy) {
  QuickAnswersState::Get()->AddObserver(observer());

  ASSERT_FALSE(
      prefs()->IsManagedPreference(quick_answers::prefs::kQuickAnswersEnabled));
  ASSERT_FALSE(prefs()->GetBoolean(quick_answers::prefs::kQuickAnswersEnabled));
  ASSERT_EQ(quick_answers::prefs::ConsentStatus::kUnknown,
            observer()->consent_status());

  prefs()->SetManagedPref(quick_answers::prefs::kQuickAnswersEnabled,
                          base::Value(false));
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kRejected,
            observer()->consent_status());
  EXPECT_FALSE(observer()->settings_enabled());
}

TEST_F(QuickAnswersStateAshTest, EnabledThenDisabledByPolicy) {
  QuickAnswersState::Get()->AddObserver(observer());

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  ASSERT_FALSE(
      prefs()->IsManagedPreference(quick_answers::prefs::kQuickAnswersEnabled));
  ASSERT_TRUE(prefs()->GetBoolean(quick_answers::prefs::kQuickAnswersEnabled));
  ASSERT_EQ(quick_answers::prefs::ConsentStatus::kAccepted,
            observer()->consent_status());

  prefs()->SetManagedPref(quick_answers::prefs::kQuickAnswersEnabled,
                          base::Value(false));
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kRejected,
            observer()->consent_status());
  EXPECT_FALSE(observer()->settings_enabled());
}

// This is for testing `turned_on` in
// `QuickAnswersStateAsh::UpdateSettingsEnabled`.
TEST_F(QuickAnswersStateAshEnabledTest, EnabledFromBeginning) {
  ASSERT_TRUE(prefs()->GetBoolean(quick_answers::prefs::kQuickAnswersEnabled));

  EXPECT_EQ(
      ConsentStatus::kUnknown,
      prefs()->GetInteger(quick_answers::prefs::kQuickAnswersConsentStatus))
      << "If pref value is enabled from beginning, it should not be treated as "
         "turned on, i.e., consent status must be un-touched.";
}

TEST_P(QuickAnswersStateAshTest, ForceDisabledForKiosk) {
  QuickAnswersState::Get()->AddObserver(observer());

  user_manager::ScopedUserManager user_manager(
      std::make_unique<user_manager::FakeUserManager>());
  chromeos::SetUpFakeKioskSession();

  EXPECT_TRUE(chromeos::IsKioskSession());
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, GetParam());

  EXPECT_FALSE(
      prefs()->IsManagedPreference(quick_answers::prefs::kQuickAnswersEnabled));
  EXPECT_EQ(quick_answers::prefs::ConsentStatus::kRejected,
            observer()->consent_status());
  EXPECT_FALSE(observer()->settings_enabled());
}

INSTANTIATE_TEST_SUITE_P(ForceDisabledForKiosk,
                         QuickAnswersStateAshTest,
                         testing::Bool());
