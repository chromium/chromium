// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_auth_local_state_pref_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

const char kUser1[] = "songttim@gmail.com";
const bool kIsChromeOSLoginEnabled1 = true;
const bool kIsEasyUnlockAllowed1 = true;
const bool kIsEasyUnlockEnabled1 = true;

const char kUser2[] = "tengs@google.com";
const bool kIsChromeOSLoginEnabled2 = false;
const bool kIsEasyUnlockAllowed2 = false;
const bool kIsEasyUnlockEnabled2 = false;

const char kUnknownUser[] = "tengs@chromium.org";
const bool kIsChromeOSLoginEnabledDefault = false;
const bool kIsEasyUnlockAllowedDefault = true;
const bool kIsEasyUnlockEnabledDefault = false;

}  //  namespace

class ProximityAuthLocalStatePrefManagerTest : public testing::Test {
 protected:
  ProximityAuthLocalStatePrefManagerTest()
      : user1_(AccountId::FromUserEmail(kUser1)),
        user2_(AccountId::FromUserEmail(kUser2)),
        unknown_user_(AccountId::FromUserEmail(kUnknownUser)) {}

  ~ProximityAuthLocalStatePrefManagerTest() override {}

  void SetUp() override {
    ProximityAuthLocalStatePrefManager::RegisterPrefs(local_state_.registry());

    // Note: in normal circumstances, these prefs are synced to local state in
    // ProximityAuthProfilePrefService.
    std::unique_ptr<base::DictionaryValue> user1_prefs(
        new base::DictionaryValue());
    user1_prefs->SetKey(
        proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
        base::Value(kIsChromeOSLoginEnabled1));
    user1_prefs->SetKey(chromeos::multidevice_setup::kSmartLockAllowedPrefName,
                        base::Value(kIsEasyUnlockAllowed1));
    user1_prefs->SetKey(chromeos::multidevice_setup::kSmartLockEnabledPrefName,
                        base::Value(kIsEasyUnlockEnabled1));
    DictionaryPrefUpdate update1(&local_state_,
                                 prefs::kEasyUnlockLocalStateUserPrefs);
    update1->SetWithoutPathExpansion(user1_.GetUserEmail(),
                                     std::move(user1_prefs));

    std::unique_ptr<base::DictionaryValue> user2_prefs(
        new base::DictionaryValue());
    user2_prefs->SetKey(
        proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
        base::Value(kIsChromeOSLoginEnabled2));
    user2_prefs->SetKey(chromeos::multidevice_setup::kSmartLockAllowedPrefName,
                        base::Value(kIsEasyUnlockAllowed2));
    user2_prefs->SetKey(chromeos::multidevice_setup::kSmartLockEnabledPrefName,
                        base::Value(kIsEasyUnlockEnabled2));

    DictionaryPrefUpdate update2(&local_state_,
                                 prefs::kEasyUnlockLocalStateUserPrefs);
    update2->SetWithoutPathExpansion(user2_.GetUserEmail(),
                                     std::move(user2_prefs));
  }

  AccountId user1_;
  AccountId user2_;
  AccountId unknown_user_;
  TestingPrefServiceSimple local_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthLocalStatePrefManagerTest);
};

TEST_F(ProximityAuthLocalStatePrefManagerTest, RegisterPrefs) {
  EXPECT_TRUE(
      local_state_.FindPreference(prefs::kEasyUnlockLocalStateUserPrefs));
}

TEST_F(ProximityAuthLocalStatePrefManagerTest, IsEasyUnlockAllowed) {
  ProximityAuthLocalStatePrefManager pref_manager(&local_state_);

  // If no active user is set, return the default value.
  EXPECT_EQ(kIsEasyUnlockAllowedDefault, pref_manager.IsEasyUnlockAllowed());

  // Unknown users should return the default value.
  pref_manager.SetActiveUser(unknown_user_);
  EXPECT_EQ(kIsEasyUnlockAllowedDefault, pref_manager.IsEasyUnlockAllowed());

  // Test users with set values.
  pref_manager.SetActiveUser(user1_);
  EXPECT_EQ(kIsEasyUnlockAllowed1, pref_manager.IsEasyUnlockAllowed());
  pref_manager.SetActiveUser(user2_);
  EXPECT_EQ(kIsEasyUnlockAllowed2, pref_manager.IsEasyUnlockAllowed());
}

TEST_F(ProximityAuthLocalStatePrefManagerTest, IsEasyUnlockEnabled) {
  ProximityAuthLocalStatePrefManager pref_manager(&local_state_);

  // If no active user is set, return the default value.
  EXPECT_EQ(kIsEasyUnlockEnabledDefault, pref_manager.IsEasyUnlockEnabled());

  // Unknown users should return the default value.
  pref_manager.SetActiveUser(unknown_user_);
  EXPECT_EQ(kIsEasyUnlockEnabledDefault, pref_manager.IsEasyUnlockEnabled());

  // Test users with set values.
  pref_manager.SetActiveUser(user1_);
  EXPECT_EQ(kIsEasyUnlockEnabled1, pref_manager.IsEasyUnlockEnabled());
  pref_manager.SetActiveUser(user2_);
  EXPECT_EQ(kIsEasyUnlockEnabled2, pref_manager.IsEasyUnlockEnabled());
}

TEST_F(ProximityAuthLocalStatePrefManagerTest, IsChromeOSLoginEnabled) {
  ProximityAuthLocalStatePrefManager pref_manager(&local_state_);

  // If no active user is set, return the default value.
  EXPECT_EQ(kIsChromeOSLoginEnabledDefault,
            pref_manager.IsChromeOSLoginEnabled());

  // Unknown users should return the default value.
  pref_manager.SetActiveUser(unknown_user_);
  EXPECT_EQ(kIsChromeOSLoginEnabledDefault,
            pref_manager.IsChromeOSLoginEnabled());

  // Test users with set values.
  pref_manager.SetActiveUser(user1_);
  EXPECT_EQ(kIsChromeOSLoginEnabled1, pref_manager.IsChromeOSLoginEnabled());
  pref_manager.SetActiveUser(user2_);
  EXPECT_EQ(kIsChromeOSLoginEnabled2, pref_manager.IsChromeOSLoginEnabled());
}

}  // namespace proximity_auth
