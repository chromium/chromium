// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/nfc_permission_context_android.h"

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "components/permissions/android/nfc/nfc_system_level_setting_impl.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace permissions {

NfcPermissionContextAndroid::NfcPermissionContextAndroid(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : NfcPermissionContext(browser_context, std::move(delegate)),
      nfc_system_level_setting_(std::make_unique<NfcSystemLevelSettingImpl>()) {
}

NfcPermissionContextAndroid::~NfcPermissionContextAndroid() = default;

void NfcPermissionContextAndroid::NotifyPermissionSet(
    const PermissionRequestData& request_data,
    BrowserPermissionCallback callback,
    bool persist,
    PermissionDecision decision,
    bool is_final_decision) {
  DCHECK(is_final_decision);

  if (decision != PermissionDecision::kAllow ||
      !nfc_system_level_setting_->IsNfcAccessPossible() ||
      nfc_system_level_setting_->IsNfcSystemLevelSettingEnabled()) {
    NfcPermissionContext::NotifyPermissionSet(request_data, std::move(callback),
                                              persist, decision,
                                              is_final_decision);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              request_data.id.global_render_frame_host_id()));

  // Ignore when the associated RenderFrameHost has already been destroyed.
  if (!web_contents)
    return;

  // Only show the NFC system level setting prompt if the tab for |web_contents|
  // is user-interactable (i.e. is the current tab, and Chrome is active and not
  // in tab-switching mode).
  if (!delegate_->IsInteractable(web_contents)) {
    ContentSettingPermissionContextBase::NotifyPermissionSet(
        request_data, std::move(callback), false /* persist */,
        PermissionDecision::kDeny, is_final_decision);
    return;
  }

  nfc_system_level_setting_->PromptToEnableNfcSystemLevelSetting(
      web_contents,
      base::BindOnce(
          &NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed,
          weak_factory_.GetWeakPtr(), request_data.id,
          request_data.requesting_origin, request_data.embedding_origin,
          std::move(callback), persist, decision));
}

void NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    PermissionDecision decision) {
  NfcPermissionContext::NotifyPermissionSet(
      PermissionRequestData(this, id,
                            content::PermissionRequestDescription(
                                content::PermissionDescriptorUtil::
                                    CreatePermissionDescriptorForPermissionType(
                                        blink::PermissionType::NFC)),
                            requesting_origin, embedding_origin),
      std::move(callback), persist, decision, /*is_final_decision=*/true);
}

}  // namespace permissions
