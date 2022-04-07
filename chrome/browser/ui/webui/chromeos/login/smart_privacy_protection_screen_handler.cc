// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/smart_privacy_protection_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/smart_privacy_protection_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId SmartPrivacyProtectionView::kScreenId;

SmartPrivacyProtectionScreenHandler::SmartPrivacyProtectionScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.SmartPrivacyProtectionScreen.userActed");
}

SmartPrivacyProtectionScreenHandler::~SmartPrivacyProtectionScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void SmartPrivacyProtectionScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }
  ShowInWebUI();
}

void SmartPrivacyProtectionScreenHandler::Hide() {
  show_on_init_ = false;
}

void SmartPrivacyProtectionScreenHandler::Bind(
    ash::SmartPrivacyProtectionScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
}

void SmartPrivacyProtectionScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

void SmartPrivacyProtectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("smartPrivacyProtectionScreenTitle",
               IDS_SMART_PRIVACY_PROTECTION_TITLE);
  builder->Add("smartPrivacyProtectionScreenLock",
               IDS_SMART_PRIVACY_PROTECTION_SCREEN_LOCK);
  builder->Add("smartPrivacyProtectionScreenLockDesc",
               IDS_SMART_PRIVACY_PROTECTION_SCREEN_LOCK_DESCRIPTION);
  builder->Add("smartPrivacyProtectionSnoopingDetection",
               IDS_SMART_PRIVACY_PROTECTION_SNOOPING_DETECTION);
  builder->Add("smartPrivacyProtectionSnoopingDetectionDesc",
               IDS_SMART_PRIVACY_PROTECTION_SNOOPING_DETECTION_DESCRIPTION);
  builder->Add("smartPrivacyProtectionContent",
               IDS_SMART_PRIVACY_PROTECTION_CONTENT);
  builder->Add("smartPrivacyProtectionTurnOnButton",
               IDS_SMART_PRIVACY_PROTECTION_TURN_ON_BUTTON);
  builder->Add("smartPrivacyProtectionTurnOffButton",
               IDS_SMART_PRIVACY_PROTECTION_TURN_OFF_BUTTON);
}

void SmartPrivacyProtectionScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  dict->Set("isQuickDimEnabled",
            base::Value(ash::features::IsQuickDimEnabled()));
  dict->Set("isSnoopingProtectionEnabled",
            base::Value(ash::features::IsSnoopingProtectionEnabled()));
}

void SmartPrivacyProtectionScreenHandler::InitializeDeprecated() {
  if (!IsJavascriptAllowed() || !screen_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
