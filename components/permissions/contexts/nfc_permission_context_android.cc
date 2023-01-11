// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/nfc_permission_context_android.h"

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "components/permissions/android/nfc/nfc_system_level_setting_impl.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

NfcPermissionContextAndroid::NfcPermissionContextAndroid(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : NfcPermissionContext(browser_context, std::move(delegate)),
      nfc_system_level_setting_(std::make_unique<NfcSystemLevelSettingImpl>()) {
}

NfcPermissionContextAndroid::~NfcPermissionContextAndroid() = default;

void NfcPermissionContextAndroid::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK(!is_one_time);
  DCHECK(is_final_decision);

  if (content_setting != CONTENT_SETTING_ALLOW ||
      !nfc_system_level_setting_->IsNfcAccessPossible() ||
      nfc_system_level_setting_->IsNfcSystemLevelSettingEnabled()) {
    NfcPermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedding_origin, std::move(callback), persist,
        content_setting, is_one_time, is_final_decision);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.global_render_frame_host_id()));

  // Ignore when the associated RenderFrameHost has already been destroyed.
  if (!web_contents)
    return;

  // Only show the NFC system level setting prompt if the tab for |web_contents|
  // is user-interactable (i.e. is the current tab, and Chrome is active and not
  // in tab-switching mode).
  if (!delegate_->IsInteractable(web_contents)) {
    PermissionContextBase::NotifyPermissionSet(
        id, requesting_origin, embedding_origin, std::move(callback),
        false /* persist */, CONTENT_SETTING_BLOCK, /*is_one_time=*/false,
        is_final_decision);
    return;
  }

  nfc_system_level_setting_->PromptToEnableNfcSystemLevelSetting(
      web_contents,
      base::BindOnce(
          &NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed,
          weak_factory_.GetWeakPtr(), id, requesting_origin, embedding_origin,
          std::move(callback), persist, content_setting));
}

void NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  NfcPermissionContext::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, /*is_one_time=*/false, /*is_final_decision=*/true);
}

}  // namespace permissions
