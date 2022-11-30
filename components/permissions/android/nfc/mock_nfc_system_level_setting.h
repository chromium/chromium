// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_NFC_MOCK_NFC_SYSTEM_LEVEL_SETTING_H_
#define COMPONENTS_PERMISSIONS_ANDROID_NFC_MOCK_NFC_SYSTEM_LEVEL_SETTING_H_

#include "components/permissions/android/nfc/nfc_system_level_setting.h"

namespace permissions {

// Mock implementation of NfcSystemLevelSetting for unit tests.
class MockNfcSystemLevelSetting : public NfcSystemLevelSetting {
 public:
  MockNfcSystemLevelSetting();

  MockNfcSystemLevelSetting(const MockNfcSystemLevelSetting&) = delete;
  MockNfcSystemLevelSetting& operator=(const MockNfcSystemLevelSetting&) =
      delete;

  ~MockNfcSystemLevelSetting() override;

  static void SetNfcAccessIsPossible(bool is_possible);
  static void SetNfcSystemLevelSettingEnabled(bool is_enabled);
  static bool HasShownNfcSettingPrompt();
  static void ClearHasShownNfcSettingPrompt();

  // NfcSystemLevelSetting implementation:
  bool IsNfcAccessPossible() override;
  bool IsNfcSystemLevelSettingEnabled() override;
  void PromptToEnableNfcSystemLevelSetting(
      content::WebContents* web_contents,
      base::OnceClosure prompt_completed_callback) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_NFC_MOCK_NFC_SYSTEM_LEVEL_SETTING_H_
