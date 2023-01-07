// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/prefs.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {

TEST(PrefsTest, RegisterProfilePrefs) {
  TestingPrefServiceSimple pref_service;

  prefs::RegisterProfilePrefs(pref_service.registry());

  EXPECT_TRUE(pref_service.FindPreference(prefs::kAutofillAssistantConsent));
  EXPECT_TRUE(pref_service.FindPreference(prefs::kAutofillAssistantEnabled));
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kAutofillAssistantTriggerScriptsEnabled));
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser));
}

TEST(PrefsTest, RegisterProfilePrefsSetCorrectDefaultValues) {
  TestingPrefServiceSimple pref_service;

  prefs::RegisterProfilePrefs(pref_service.registry());

  EXPECT_FALSE(pref_service.GetBoolean(prefs::kAutofillAssistantConsent));
  EXPECT_TRUE(pref_service.GetBoolean(prefs::kAutofillAssistantEnabled));
  EXPECT_TRUE(
      pref_service.GetBoolean(prefs::kAutofillAssistantTriggerScriptsEnabled));
  EXPECT_TRUE(pref_service.GetBoolean(
      prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser));
}

}  // namespace autofill_assistant
