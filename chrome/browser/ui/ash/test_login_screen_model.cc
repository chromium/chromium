// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_login_screen_model.h"

TestLoginScreenModel::TestLoginScreenModel() = default;
TestLoginScreenModel::~TestLoginScreenModel() = default;

void TestLoginScreenModel::SetUserList(
    const std::vector<ash::LoginUserInfo>& users) {}
void TestLoginScreenModel::SetPinEnabledForUser(const AccountId& account_id,
                                                bool is_enabled) {}
void TestLoginScreenModel::SetFingerprintState(const AccountId& account_id,
                                               ash::FingerprintState state) {}
void TestLoginScreenModel::SetAvatarForUser(const AccountId& account_id,
                                            const ash::UserAvatar& avatar) {}
void TestLoginScreenModel::NotifyFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {}
void TestLoginScreenModel::EnableAuthForUser(const AccountId& account_id) {}
void TestLoginScreenModel::DisableAuthForUser(
    const AccountId& account_id,
    const ash::AuthDisabledData& auth_disabled_data) {}
void TestLoginScreenModel::SetTapToUnlockEnabledForUser(
    const AccountId& account_id,
    bool enabled) {}
void TestLoginScreenModel::ForceOnlineSignInForUser(
    const AccountId& account_id) {}
void TestLoginScreenModel::ShowEasyUnlockIcon(
    const AccountId& account_id,
    const ash::EasyUnlockIconOptions& icon) {}
void TestLoginScreenModel::SetChallengeResponseAuthEnabledForUser(
    const AccountId& user,
    bool enabled) {}
void TestLoginScreenModel::UpdateWarningMessage(const base::string16& message) {
}
void TestLoginScreenModel::SetSystemInfo(
    bool show,
    bool enforced,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name,
    bool adb_sideloading_enabled) {}
void TestLoginScreenModel::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {}
void TestLoginScreenModel::SetPublicSessionLocales(
    const AccountId& account_id,
    const std::vector<ash::LocaleItem>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {}
void TestLoginScreenModel::SetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<ash::InputMethodItem>& keyboard_layouts) {}
void TestLoginScreenModel::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {}
void TestLoginScreenModel::HandleFocusLeavingLockScreenApps(bool reverse) {}
void TestLoginScreenModel::NotifyOobeDialogState(ash::OobeDialogState state) {}
