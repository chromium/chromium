// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_NFC_PERMISSION_CONTEXT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_NFC_PERMISSION_CONTEXT_ANDROID_H_

#include "components/permissions/android/nfc/nfc_system_level_setting.h"
#include "components/permissions/contexts/nfc_permission_context.h"

namespace permissions {

class PermissionRequestID;

class NfcPermissionContextAndroid : public NfcPermissionContext {
 public:
  NfcPermissionContextAndroid(content::BrowserContext* browser_context,
                              std::unique_ptr<Delegate> delegate);
  ~NfcPermissionContextAndroid() override;

 private:
  friend class NfcPermissionContextTests;

  // NfcPermissionContext:
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override;

  void OnNfcSystemLevelSettingPromptClosed(const PermissionRequestID& id,
                                           const GURL& requesting_origin,
                                           const GURL& embedding_origin,
                                           BrowserPermissionCallback callback,
                                           bool persist,
                                           ContentSetting content_setting);

  // Overrides the NfcSystemLevelSetting object used to determine whether NFC is
  // enabled system-wide on the device.
  void set_nfc_system_level_setting_for_testing(
      std::unique_ptr<NfcSystemLevelSetting> nfc_system_level_setting) {
    nfc_system_level_setting_ = std::move(nfc_system_level_setting);
  }

  std::unique_ptr<NfcSystemLevelSetting> nfc_system_level_setting_;

  base::WeakPtrFactory<NfcPermissionContextAndroid> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_NFC_PERMISSION_CONTEXT_ANDROID_H_
