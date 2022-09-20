// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/preference_manager.h"

#include "build/buildflag.h"
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

}  // namespace autofill_assistant
