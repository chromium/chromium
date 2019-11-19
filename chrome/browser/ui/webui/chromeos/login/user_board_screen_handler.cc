// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_board_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId UserBoardView::kScreenId;

UserBoardScreenHandler::UserBoardScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {}

UserBoardScreenHandler::~UserBoardScreenHandler() {
}

void UserBoardScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
}

void UserBoardScreenHandler::RegisterMessages() {
  AddCallback("attemptUnlock", &UserBoardScreenHandler::HandleAttemptUnlock);
  AddCallback("hardlockPod", &UserBoardScreenHandler::HandleHardlockPod);
}

void UserBoardScreenHandler::Initialize() {
}

//----------------- Handlers

void UserBoardScreenHandler::HandleHardlockPod(const AccountId& account_id) {
  CHECK(screen_);
  screen_->HardLockPod(account_id);
}

void UserBoardScreenHandler::HandleAttemptUnlock(const AccountId& account_id) {
  CHECK(screen_);
  screen_->AttemptEasyUnlock(account_id);
}

//----------------- API

void UserBoardScreenHandler::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  CallJS("login.AccountPickerScreen.setPublicSessionDisplayName", account_id,
         display_name);
}

void UserBoardScreenHandler::SetPublicSessionLocales(
    const AccountId& account_id,
    std::unique_ptr<base::ListValue> locales,
    const std::string& default_locale,
    bool multiple_recommended_locales) {
  CallJS("login.AccountPickerScreen.setPublicSessionLocales", account_id,
         *locales, default_locale, multiple_recommended_locales);
}

void UserBoardScreenHandler::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  // This method is only called from browser_tests and shouldn't do anything.
}

void UserBoardScreenHandler::ShowBannerMessage(const base::string16& message,
                                               bool is_warning) {
  CallJS("login.AccountPickerScreen.showBannerMessage", message, is_warning);
}

void UserBoardScreenHandler::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  std::unique_ptr<base::DictionaryValue> icon =
      icon_options.ToDictionaryValue();
  if (!icon || icon->empty())
    return;
  CallJS("login.AccountPickerScreen.showUserPodCustomIcon", account_id, *icon);
}

void UserBoardScreenHandler::HideUserPodCustomIcon(
    const AccountId& account_id) {
  CallJS("login.AccountPickerScreen.hideUserPodCustomIcon", account_id);
}

void UserBoardScreenHandler::SetAuthType(
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const base::string16& initial_value) {
  CallJS("login.AccountPickerScreen.setAuthType", account_id,
         static_cast<int>(auth_type), base::Value(initial_value));
}

void UserBoardScreenHandler::Bind(UserSelectionScreen* screen) {
  screen_ = screen;
  SetBaseScreen(screen_);
}

void UserBoardScreenHandler::Unbind() {
  screen_ = nullptr;
  SetBaseScreen(nullptr);
}

base::WeakPtr<UserBoardView> UserBoardScreenHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos
