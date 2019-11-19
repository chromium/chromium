// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_MODEL_H_
#define CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_MODEL_H_

#include "ash/public/cpp/login_screen_model.h"
#include "base/macros.h"

class TestLoginScreenModel : public ash::LoginScreenModel {
 public:
  TestLoginScreenModel();
  ~TestLoginScreenModel() override;

  // ash::LoginScreenModel:
  void SetUserList(const std::vector<ash::LoginUserInfo>& users) override;
  void SetPinEnabledForUser(const AccountId& account_id,
                            bool is_enabled) override;
  void SetFingerprintState(const AccountId& account_id,
                           ash::FingerprintState state) override;
  void SetAvatarForUser(const AccountId& account_id,
                        const ash::UserAvatar& avatar) override;
  void NotifyFingerprintAuthResult(const AccountId& account_id,
                                   bool successful) override;
  void EnableAuthForUser(const AccountId& account_id) override;
  void DisableAuthForUser(
      const AccountId& account_id,
      const ash::AuthDisabledData& auth_disabled_data) override;
  void SetTapToUnlockEnabledForUser(const AccountId& account_id,
                                    bool enabled) override;
  void ForceOnlineSignInForUser(const AccountId& account_id) override;
  void ShowEasyUnlockIcon(const AccountId& user,
                          const ash::EasyUnlockIconOptions& icon) override;
  void SetChallengeResponseAuthEnabledForUser(const AccountId& user,
                                              bool enabled) override;

  void UpdateWarningMessage(const base::string16& message) override;
  void SetSystemInfo(bool show,
                     bool enforced,
                     const std::string& os_version_label_text,
                     const std::string& enterprise_info_text,
                     const std::string& bluetooth_name,
                     bool adb_sideloading_enabled) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               const std::vector<ash::LocaleItem>& locales,
                               const std::string& default_locale,
                               bool show_advanced_view) override;
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<ash::InputMethodItem>& keyboard_layouts) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void HandleFocusLeavingLockScreenApps(bool reverse) override;
  void NotifyOobeDialogState(ash::OobeDialogState state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLoginScreenModel);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_MODEL_H_
