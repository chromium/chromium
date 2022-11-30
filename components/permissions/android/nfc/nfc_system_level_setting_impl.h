// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_NFC_NFC_SYSTEM_LEVEL_SETTING_IMPL_H_
#define COMPONENTS_PERMISSIONS_ANDROID_NFC_NFC_SYSTEM_LEVEL_SETTING_IMPL_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/permissions/android/nfc/nfc_system_level_setting.h"

namespace permissions {

class NfcSystemLevelSettingImpl : public NfcSystemLevelSetting {
 public:
  NfcSystemLevelSettingImpl();

  NfcSystemLevelSettingImpl(const NfcSystemLevelSettingImpl&) = delete;
  NfcSystemLevelSettingImpl& operator=(const NfcSystemLevelSettingImpl&) =
      delete;

  ~NfcSystemLevelSettingImpl() override;

  // NfcSystemLevelSetting implementation:
  bool IsNfcAccessPossible() override;
  bool IsNfcSystemLevelSettingEnabled() override;
  void PromptToEnableNfcSystemLevelSetting(
      content::WebContents* web_contents,
      base::OnceClosure prompt_completed_callback) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_NFC_NFC_SYSTEM_LEVEL_SETTING_IMPL_H_
