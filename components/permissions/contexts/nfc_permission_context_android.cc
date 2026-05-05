// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/nfc_permission_context_android.h"

#include <memory>
#include <variant>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "components/permissions/android/nfc/nfc_system_level_setting_impl.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_data.h"
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
    const content::PermissionResult* permission_result,
    const permissions::PermissionPromptDecision& decision) {
  CHECK(decision.is_final);

  if (decision.overall_decision != PermissionDecision::kAllow ||
      !nfc_system_level_setting_->IsNfcAccessPossible() ||
      nfc_system_level_setting_->IsNfcSystemLevelSettingEnabled()) {
    NfcPermissionContext::NotifyPermissionSet(request_data, std::move(callback),
                                              persist, permission_result,
                                              decision);
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
        request_data, std::move(callback), /*persist=*/false,
        // Force computing a new PermissionResult since we are denying the
        // permission request.
        nullptr,
        permissions::PermissionPromptDecision{PermissionDecision::kDeny,
                                              decision.prompt_options,
                                              decision.is_final});
    return;
  }

  nfc_system_level_setting_->PromptToEnableNfcSystemLevelSetting(
      web_contents,
      base::BindOnce(
          &NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed,
          weak_factory_.GetWeakPtr(), request_data.Clone(), std::move(callback),
          persist,
          permission_result
              ? std::make_unique<content::PermissionResult>(*permission_result)
              : nullptr,
          decision));
}

void NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed(
    const PermissionRequestData& request_data,
    BrowserPermissionCallback callback,
    bool persist,
    std::unique_ptr<content::PermissionResult> permission_result,
    const permissions::PermissionPromptDecision& decision) {
  NfcPermissionContext::NotifyPermissionSet(request_data, std::move(callback),
                                            persist, permission_result.get(),
                                            decision);
}

}  // namespace permissions
