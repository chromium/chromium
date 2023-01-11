// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/spell_checker.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

class SpellCheckerTest : public QuickAnswersTestBase {
 public:
  SpellCheckerTest() = default;
  SpellCheckerTest(const SpellCheckerTest&) = delete;
  SpellCheckerTest& operator=(const SpellCheckerTest&) = delete;
  ~SpellCheckerTest() override = default;

  // QuickAnswersTestBase:
  void SetUp() override {
    QuickAnswersTestBase::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    spell_checker_ =
        std::make_unique<SpellChecker>(test_shared_loader_factory_);
  }

  void TearDown() override {
    spell_checker_.reset();
    QuickAnswersTestBase::TearDown();
  }

  SpellChecker* spell_checker() { return spell_checker_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<SpellChecker> spell_checker_;
};

TEST_F(SpellCheckerTest, ShouldNotSetupBeforePrefsInitialized) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldNotSetupIfFeatureDisabled) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(false);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldResetOnFeatureDisabled) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_TRUE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->SetSettingsEnabled(false);

  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldSetupIfShouldShowUserConsent) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(false);
  // We should show user consent UI if consent status is kUnknown.
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kUnknown);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_TRUE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldResetOnUserConsentRejected) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(false);
  // We should show user consent UI if consent status is kUnknown.
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kUnknown);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_TRUE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kRejected);

  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldNotSetupWithUnsupportedApplicationLocale) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("zh");
  fake_quick_answers_state()->SetPreferredLanguages("zh,en");

  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldSetupWithSupportedApplicationLocale) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_TRUE(spell_checker()->GetSpellcheckLanguagesForTesting().size());
}

TEST_F(SpellCheckerTest, ShouldFilterCountryCodeOfApplicationLocale) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("en-US");
  fake_quick_answers_state()->SetPreferredLanguages("en-US,en-GB");

  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting().size(), 1u);
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[0]->language(),
            "en");
}

TEST_F(SpellCheckerTest, ShouldSetupWithPreferredLanguages) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,es");

  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting().size(), 2u);
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[0]->language(),
            "en");
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[1]->language(),
            "es");

  fake_quick_answers_state()->SetPreferredLanguages("en,es,it");

  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting().size(), 3u);
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[0]->language(),
            "en");
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[1]->language(),
            "es");
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[2]->language(),
            "it");
}

TEST_F(SpellCheckerTest, ShouldFilterUnsupportedPreferredLanguages) {
  EXPECT_FALSE(fake_quick_answers_state()->prefs_initialized());
  EXPECT_FALSE(spell_checker()->GetSpellcheckLanguagesForTesting().size());

  fake_quick_answers_state()->OnPrefsInitialized();
  fake_quick_answers_state()->SetSettingsEnabled(true);
  fake_quick_answers_state()->SetConsentStatus(prefs::ConsentStatus::kAccepted);
  fake_quick_answers_state()->SetApplicationLocale("en");
  fake_quick_answers_state()->SetPreferredLanguages("en,zh,es");

  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting().size(), 2u);
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[0]->language(),
            "en");
  EXPECT_EQ(spell_checker()->GetSpellcheckLanguagesForTesting()[1]->language(),
            "es");
}

}  // namespace quick_answers
