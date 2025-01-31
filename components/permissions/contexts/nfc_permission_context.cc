// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/nfc_permission_context.h"

#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

NfcPermissionContext::NfcPermissionContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::NFC,
                            blink::mojom::PermissionsPolicyFeature::kNotFound),
      delegate_(std::move(delegate)) {}

NfcPermissionContext::~NfcPermissionContext() = default;

#if !BUILDFLAG(IS_ANDROID)
ContentSetting NfcPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_BLOCK;
}
#endif

void NfcPermissionContext::DecidePermission(
    PermissionRequestData request_data,
    BrowserPermissionCallback callback) {
  if (!request_data.user_gesture) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }
  permissions::PermissionContextBase::DecidePermission(std::move(request_data),
                                                       std::move(callback));
}

void NfcPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                            const GURL& requesting_frame,
                                            bool allowed) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());
  if (!content_settings)
    return;

  if (allowed)
    content_settings->OnContentAllowed(ContentSettingsType::NFC);
  else
    content_settings->OnContentBlocked(ContentSettingsType::NFC);
}

}  // namespace permissions
