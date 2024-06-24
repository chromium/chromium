// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

enum SettingManager { USER, ADMIN, EXTENSION };

class SafeBrowsingResultTest : public testing::Test {
 public:
  SafeBrowsingResultTest() = default;
  ~SafeBrowsingResultTest() override = default;

  void SetPrefsForSafeBrowsing(bool is_enabled,
                               bool is_enhanced,
                               SettingManager managed_by) {
    auto* prefs = profile()->GetTestingPrefService();
    switch (managed_by) {
      case USER:
        prefs->SetUserPref(prefs::kSafeBrowsingEnabled,
                           std::make_unique<base::Value>(is_enabled));
        prefs->SetUserPref(prefs::kSafeBrowsingEnhanced,
                           std::make_unique<base::Value>(is_enhanced));
        break;
      case ADMIN:
        prefs->SetManagedPref(prefs::kSafeBrowsingEnabled,
                              std::make_unique<base::Value>(is_enabled));
        prefs->SetManagedPref(prefs::kSafeBrowsingEnhanced,
                              std::make_unique<base::Value>(is_enhanced));
        break;
      case EXTENSION:
        prefs->SetExtensionPref(prefs::kSafeBrowsingEnabled,
                                std::make_unique<base::Value>(is_enabled));
        prefs->SetExtensionPref(prefs::kSafeBrowsingEnhanced,
                                std::make_unique<base::Value>(is_enhanced));
        break;
      default:
        NOTREACHED() << "Unexpected value for managed_by argument. \n";
    }
  }

 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment environment;
  TestingProfile profile_;
};

TEST_F(SafeBrowsingResultTest, CloneResult) {
  // When creating a result where the status is set to disabled, which is a
  // trigger for a menu notification, it should remain as such even after it was
  // turned into Dict.
  auto result = std::make_unique<SafetyHubSafeBrowsingResult>(
      SafeBrowsingState::kDisabledByUser);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());

  auto cloned_result = result->Clone();
  EXPECT_TRUE(cloned_result->IsTriggerForMenuNotification());
}

TEST_F(SafeBrowsingResultTest, ResultIsTrigger) {
  // Only a disabled status should result in a menu notification.
  for (int i = 0; i <= int(SafeBrowsingState::kMaxValue); i++) {
    auto status = static_cast<SafeBrowsingState>(i);
    auto result = std::make_unique<SafetyHubSafeBrowsingResult>(status);
    if (status == SafeBrowsingState::kDisabledByUser) {
      EXPECT_TRUE(result->IsTriggerForMenuNotification());
      continue;
    }
    EXPECT_FALSE(result->IsTriggerForMenuNotification());
  }
}

TEST_F(SafeBrowsingResultTest, GetState_EnabledEnhanced) {
  SetPrefsForSafeBrowsing(true, true, SettingManager::USER);
  EXPECT_EQ(SafeBrowsingState::kEnabledEnhanced,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(true, true, SettingManager::EXTENSION);
  EXPECT_EQ(SafeBrowsingState::kEnabledEnhanced,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(true, true, SettingManager::ADMIN);
  EXPECT_EQ(SafeBrowsingState::kEnabledEnhanced,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));
}

TEST_F(SafeBrowsingResultTest, GetState_EnabledStandard) {
  SetPrefsForSafeBrowsing(true, false, SettingManager::USER);
  EXPECT_EQ(SafeBrowsingState::kEnabledStandard,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(true, false, SettingManager::EXTENSION);
  EXPECT_EQ(SafeBrowsingState::kEnabledStandard,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(true, false, SettingManager::ADMIN);
  EXPECT_EQ(SafeBrowsingState::kEnabledStandard,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));
}

TEST_F(SafeBrowsingResultTest, GetState_DisabledByAdmin) {
  SetPrefsForSafeBrowsing(false, false, SettingManager::ADMIN);
  EXPECT_EQ(SafeBrowsingState::kDisabledByAdmin,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(false, true, SettingManager::ADMIN);
  EXPECT_EQ(SafeBrowsingState::kDisabledByAdmin,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));
}

TEST_F(SafeBrowsingResultTest, GetState_DisabledByExtension) {
  SetPrefsForSafeBrowsing(false, false, SettingManager::EXTENSION);
  EXPECT_EQ(SafeBrowsingState::kDisabledByExtension,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(false, true, SettingManager::EXTENSION);
  EXPECT_EQ(SafeBrowsingState::kDisabledByExtension,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));
}

TEST_F(SafeBrowsingResultTest, GetState_DisabledByUser) {
  SetPrefsForSafeBrowsing(false, false, SettingManager::USER);
  EXPECT_EQ(SafeBrowsingState::kDisabledByUser,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));

  SetPrefsForSafeBrowsing(false, true, SettingManager::USER);
  EXPECT_EQ(SafeBrowsingState::kDisabledByUser,
            SafetyHubSafeBrowsingResult::GetState(
                profile()->GetTestingPrefService()));
}
