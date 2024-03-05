// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/smart_privacy_protection_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

SmartPrivacyProtectionScreenHandler::SmartPrivacyProtectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

SmartPrivacyProtectionScreenHandler::~SmartPrivacyProtectionScreenHandler() =
    default;

void SmartPrivacyProtectionScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<SmartPrivacyProtectionView>
SmartPrivacyProtectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SmartPrivacyProtectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("smartPrivacyProtectionScreenTitle",
               IDS_SMART_PRIVACY_PROTECTION_TITLE);
  builder->Add("smartPrivacyProtectionScreenLock",
               IDS_SMART_PRIVACY_PROTECTION_SCREEN_LOCK);
  builder->Add("smartPrivacyProtectionScreenLockDesc",
               IDS_SMART_PRIVACY_PROTECTION_SCREEN_LOCK_DESCRIPTION);
  builder->Add("smartPrivacyProtectionContent",
               IDS_SMART_PRIVACY_PROTECTION_CONTENT);
  builder->Add("smartPrivacyProtectionTurnOnButton",
               IDS_SMART_PRIVACY_PROTECTION_TURN_ON_BUTTON);
  builder->Add("smartPrivacyProtectionTurnOffButton",
               IDS_SMART_PRIVACY_PROTECTION_TURN_OFF_BUTTON);
}

void SmartPrivacyProtectionScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  dict->Set("isQuickDimEnabled", base::Value(features::IsQuickDimEnabled()));
}

}  // namespace ash
