// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_settings_service_impl.h"

#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class PasswordManagerSettingsServiceImplTest : public testing::Test {
 protected:
  PasswordManagerSettingsServiceImplTest();
  ~PasswordManagerSettingsServiceImplTest() override;

  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }
  PasswordManagerSettingsServiceImpl* settings_service() {
    return &settings_service_;
  }

 private:
  TestingPrefServiceSimple test_pref_service_;
  PasswordManagerSettingsServiceImpl settings_service_{&test_pref_service_};
};

PasswordManagerSettingsServiceImplTest::
    PasswordManagerSettingsServiceImplTest() {
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableService, true);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableAutosignin, true);
}

PasswordManagerSettingsServiceImplTest::
    ~PasswordManagerSettingsServiceImplTest() = default;

TEST_F(PasswordManagerSettingsServiceImplTest, SavingPasswordsDependsOnPref) {
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));

  pref_service()->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                             false);
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kOfferToSavePasswords));
}

TEST_F(PasswordManagerSettingsServiceImplTest, AutoSignInEnabledDependsOnPref) {
  EXPECT_TRUE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));

  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  EXPECT_FALSE(settings_service()->IsSettingEnabled(
      PasswordManagerSetting::kAutoSignIn));
}

TEST_F(PasswordManagerSettingsServiceImplTest, TurnOffAutoSignIn) {
  ASSERT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));

  settings_service()->TurnOffAutoSignIn();
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
}

}  // namespace password_manager
