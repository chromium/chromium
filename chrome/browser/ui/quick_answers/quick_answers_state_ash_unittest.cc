// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_state_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace {

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
  void OnApplicationLocaleReady(
      const std::string& application_locale) override {
    application_locale_ = application_locale;
  }
  void OnPreferredLanguagesChanged(
      const std::string& preferred_languages) override {
    preferred_languages_ = preferred_languages;
  }
  void OnEligibilityChanged(bool eligible) override { is_eligible_ = eligible; }

  bool settings_enabled() const { return settings_enabled_; }
  const std::string& application_locale() const { return application_locale_; }
  const std::string& preferred_languages() const {
    return preferred_languages_;
  }
  bool is_eligible() const { return is_eligible_; }

 private:
  bool settings_enabled_ = false;
  std::string application_locale_;
  std::string preferred_languages_;
  bool is_eligible_ = false;
};

class QuickAnswersStateAshTest : public ChromeQuickAnswersTestBase {
 protected:
  QuickAnswersStateAshTest() = default;
  QuickAnswersStateAshTest(const QuickAnswersStateAshTest&) = delete;
  QuickAnswersStateAshTest& operator=(const QuickAnswersStateAshTest&) = delete;
  ~QuickAnswersStateAshTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    prefs_ =
        ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    DCHECK(prefs_);

    observer_ = std::make_unique<TestQuickAnswersStateObserver>();
  }

  PrefService* prefs() { return prefs_; }

  TestQuickAnswersStateObserver* observer() { return observer_.get(); }

 private:
  PrefService* prefs_ = nullptr;
  std::unique_ptr<TestQuickAnswersStateObserver> observer_;
};

TEST_F(QuickAnswersStateAshTest, InitObserver) {
  EXPECT_FALSE(QuickAnswersState::Get()->settings_enabled());
  EXPECT_EQ(QuickAnswersState::Get()->application_locale(), std::string());

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  const std::string application_locale = "en-US";
  prefs()->SetString(language::prefs::kApplicationLocale, application_locale);

  EXPECT_TRUE(QuickAnswersState::Get()->settings_enabled());
  EXPECT_EQ(QuickAnswersState::Get()->application_locale(), application_locale);

  // The observer class should get an instant notification about the current
  // pref value.
  QuickAnswersState::Get()->AddObserver(observer());
  EXPECT_TRUE(observer()->settings_enabled());
  EXPECT_EQ(observer()->application_locale(), application_locale);

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, NotifySettingsEnabled) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::Get()->settings_enabled());
  EXPECT_FALSE(observer()->settings_enabled());
  EXPECT_EQ(QuickAnswersState::Get()->consent_status(),
            quick_answers::prefs::ConsentStatus::kUnknown);

  // The observer class should get an notification when the pref value changes.
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  EXPECT_TRUE(QuickAnswersState::Get()->settings_enabled());
  EXPECT_TRUE(observer()->settings_enabled());

  // Consent status should also be set to accepted since the feature is
  // explicitly enabled.
  EXPECT_EQ(QuickAnswersState::Get()->consent_status(),
            quick_answers::prefs::ConsentStatus::kAccepted);

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateAshTest, UpdateConsentStatus) {
  EXPECT_EQ(QuickAnswersState::Get()->consent_status(),
            quick_answers::prefs::ConsentStatus::kUnknown);

  prefs()->SetInteger(quick_answers::prefs::kQuickAnswersConsentStatus,
                      quick_answers::prefs::ConsentStatus::kRejected);
  EXPECT_EQ(QuickAnswersState::Get()->consent_status(),
            quick_answers::prefs::ConsentStatus::kRejected);

  prefs()->SetInteger(quick_answers::prefs::kQuickAnswersConsentStatus,
                      quick_answers::prefs::ConsentStatus::kAccepted);
  EXPECT_EQ(QuickAnswersState::Get()->consent_status(),
            quick_answers::prefs::ConsentStatus::kAccepted);
}

TEST_F(QuickAnswersStateAshTest, UpdateDefinitionEnabled) {
  // Definition subfeature is default on.
  EXPECT_TRUE(QuickAnswersState::Get()->definition_enabled());

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersDefinitionEnabled,
                      false);
  EXPECT_FALSE(QuickAnswersState::Get()->definition_enabled());
}

TEST_F(QuickAnswersStateAshTest, UpdateTranslationEnabled) {
  // Translation subfeature is default on.
  EXPECT_TRUE(QuickAnswersState::Get()->translation_enabled());

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersTranslationEnabled,
                      false);
  EXPECT_FALSE(QuickAnswersState::Get()->translation_enabled());
}

TEST_F(QuickAnswersStateAshTest, UpdateUnitConversionEnabled) {
  // Unit conversion subfeature is default on.
  EXPECT_TRUE(QuickAnswersState::Get()->unit_conversion_enabled());

  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersUnitConversionEnabled,
                      false);
  EXPECT_FALSE(QuickAnswersState::Get()->unit_conversion_enabled());
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

TEST_F(QuickAnswersStateAshTest, LocaleEligible) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());

  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale(ULOC_US), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "en");

  SimulateUserLogin(kTestUser);
  EXPECT_TRUE(QuickAnswersState::Get()->is_eligible());
  EXPECT_TRUE(observer()->is_eligible());
}

TEST_F(QuickAnswersStateAshTest, EligibleLocales) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersForMoreLocales);

  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());

  prefs()->SetString(language::prefs::kApplicationLocale, "pt");
  SimulateUserLogin(kTestUser);
  EXPECT_TRUE(QuickAnswersState::Get()->is_eligible());
  EXPECT_TRUE(observer()->is_eligible());

  ClearLogin();

  prefs()->SetString(language::prefs::kApplicationLocale, "en");
  SimulateUserLogin(kTestUser);
  EXPECT_TRUE(QuickAnswersState::Get()->is_eligible());
  EXPECT_TRUE(observer()->is_eligible());
}

TEST_F(QuickAnswersStateAshTest, LocaleIneligible) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());

  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale(ULOC_CHINESE), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "zh");

  SimulateUserLogin(kTestUser);
  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());
}

TEST_F(QuickAnswersStateAshTest, IneligibleLocales) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersForMoreLocales);

  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());

  prefs()->SetString(language::prefs::kApplicationLocale, "zh");
  SimulateUserLogin(kTestUser);
  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());

  ClearLogin();

  prefs()->SetString(language::prefs::kApplicationLocale, "ja");
  SimulateUserLogin(kTestUser);
  EXPECT_FALSE(QuickAnswersState::Get()->is_eligible());
  EXPECT_FALSE(observer()->is_eligible());
}
