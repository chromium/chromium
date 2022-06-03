// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/nfc/mock_nfc_system_level_setting.h"

namespace {

static bool nfc_access_is_possible_ = false;
static bool is_nfc_setting_enabled_ = false;
static bool has_shown_nfc_setting_prompt_ = false;

}  // namespace

namespace permissions {

MockNfcSystemLevelSetting::MockNfcSystemLevelSetting()
    : NfcSystemLevelSetting() {}

MockNfcSystemLevelSetting::~MockNfcSystemLevelSetting() {}

void MockNfcSystemLevelSetting::SetNfcAccessIsPossible(bool is_possible) {
  nfc_access_is_possible_ = is_possible;
}

void MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(
    bool is_enabled) {
  is_nfc_setting_enabled_ = is_enabled;
}

bool MockNfcSystemLevelSetting::HasShownNfcSettingPrompt() {
  return has_shown_nfc_setting_prompt_;
}

void MockNfcSystemLevelSetting::ClearHasShownNfcSettingPrompt() {
  has_shown_nfc_setting_prompt_ = false;
}

bool MockNfcSystemLevelSetting::IsNfcAccessPossible() {
  return nfc_access_is_possible_;
}

bool MockNfcSystemLevelSetting::IsNfcSystemLevelSettingEnabled() {
  return is_nfc_setting_enabled_;
}

void MockNfcSystemLevelSetting::PromptToEnableNfcSystemLevelSetting(
    content::WebContents* web_contents,
    base::OnceClosure prompt_completed_callback) {
  has_shown_nfc_setting_prompt_ = true;
  std::move(prompt_completed_callback).Run();
}

}  // namespace permissions
