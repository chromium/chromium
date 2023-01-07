// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/preference_manager.h"

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {

class PreferenceManagerTest : public testing::Test {
 public:
  PreferenceManagerTest() : preference_manager_(&pref_service_) {}

  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  PrefService& pref_service() { return pref_service_; }
  PreferenceManager& preference_manager() { return preference_manager_; }

 private:
  TestingPrefServiceSimple pref_service_;
  PreferenceManager preference_manager_;
};

TEST_F(PreferenceManagerTest, GetAndSetFirstTimeTriggerScriptUser) {
  // A new user is a first time trigger script user.
  EXPECT_TRUE(preference_manager().GetIsFirstTimeTriggerScriptUser());

  preference_manager().SetIsFirstTimeTriggerScriptUser(false);
  EXPECT_FALSE(preference_manager().GetIsFirstTimeTriggerScriptUser());
}

TEST_F(PreferenceManagerTest, ProactiveHelpConditions) {
  // By default, proactive help (trigger scripts) are on.
  EXPECT_TRUE(preference_manager().IsProactiveHelpOn());

  pref_service().SetBoolean(prefs::kAutofillAssistantEnabled, false);
  EXPECT_FALSE(preference_manager().IsProactiveHelpOn());

  pref_service().SetBoolean(prefs::kAutofillAssistantEnabled, true);
  pref_service().SetBoolean(prefs::kAutofillAssistantTriggerScriptsEnabled,
                            false);
  EXPECT_FALSE(preference_manager().IsProactiveHelpOn());

  preference_manager().SetProactiveHelpSettingEnabled(true);
  EXPECT_TRUE(preference_manager().IsProactiveHelpOn());
}

TEST_F(PreferenceManagerTest, ProactiveHelpConditionsAreOffWhenFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAssistantProactiveHelp);

  EXPECT_FALSE(preference_manager().IsProactiveHelpOn());
}

TEST_F(PreferenceManagerTest, AcceptOnboarding) {
  pref_service().SetBoolean(prefs::kAutofillAssistantEnabled, false);
  preference_manager().SetOnboardingAccepted(true);

  // Accepting onboarding turns on Autofill Assistant.
  EXPECT_TRUE(preference_manager().GetOnboardingAccepted());
  EXPECT_TRUE(pref_service().GetBoolean(prefs::kAutofillAssistantConsent));
  EXPECT_TRUE(pref_service().GetBoolean(prefs::kAutofillAssistantEnabled));
}

TEST_F(PreferenceManagerTest, DeclineOnboarding) {
  // By default, Assistant is on, but consent has not been given.
  EXPECT_TRUE(pref_service().GetBoolean(prefs::kAutofillAssistantEnabled));
  EXPECT_FALSE(preference_manager().GetOnboardingAccepted());

  preference_manager().SetOnboardingAccepted(false);

  EXPECT_FALSE(pref_service().GetBoolean(prefs::kAutofillAssistantConsent));
  // Rejecting onboarding does not turn off Autofill Assistant.
  EXPECT_TRUE(pref_service().GetBoolean(prefs::kAutofillAssistantEnabled));
  EXPECT_FALSE(preference_manager().GetOnboardingAccepted());
}

TEST_F(PreferenceManagerTest, DisablingAutofillAssistantSwitchRevokesConsent) {
  preference_manager().SetOnboardingAccepted(true);
  pref_service().SetBoolean(prefs::kAutofillAssistantEnabled, false);

  EXPECT_FALSE(preference_manager().GetOnboardingAccepted());
}

}  // namespace autofill_assistant
