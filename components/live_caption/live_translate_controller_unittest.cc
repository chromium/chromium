// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_translate_controller.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captions {

class LiveTranslateControllerTest : public testing::Test {
 public:
  LiveTranslateControllerTest() = default;
  ~LiveTranslateControllerTest() override = default;

  void SetUp() override {
    LiveTranslateController::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestingPrefServiceSimple testing_pref_service_;
};

//  Test that profile prefs are registered correctly.
TEST_F(LiveTranslateControllerTest, ProfilePrefsAreRegisteredCorrectly) {
  LiveTranslateController::RegisterProfilePrefs(
      static_cast<user_prefs::PrefRegistrySyncable*>(
          testing_pref_service_.registry()));

  EXPECT_FALSE(testing_pref_service_.GetBoolean(prefs::kLiveTranslateEnabled));
  EXPECT_EQ(
      testing_pref_service_.GetString(prefs::kLiveTranslateTargetLanguageCode),
      speech::kEnglishLocaleNoCountry);
}

}  // namespace captions
