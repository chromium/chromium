// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/speaker_selection_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

SpeakerSelectionPermissionContext::SpeakerSelectionPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::SPEAKER_SELECTION,
          blink::mojom::PermissionsPolicyFeature::kSpeakerSelection) {}

ContentSetting SpeakerSelectionPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // TODO(https://crbug.com/41492674): It will be revisited when the policy
  // is enabled.
  return CONTENT_SETTING_ASK;
}

void SpeakerSelectionPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  // TODO(https://crbug.com/41492674): It will be revisited when the policy
  // is enabled.
  NotifyPermissionSet(request_data.id, request_data.requesting_origin,
                      request_data.embedding_origin, std::move(callback),
                      /*persist=*/false, CONTENT_SETTING_DEFAULT,
                      /*is_one_time=*/false,
                      /*is_final_decision=*/true);
}

void SpeakerSelectionPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  // TODO(https://crbug.com/41492674): It should continue to support of
  // implicit consent via getUserMedia().
  // https://w3c.github.io/mediacapture-output/#privacy-obtaining-consent)
  NOTIMPLEMENTED();
}

}  // namespace permissions
