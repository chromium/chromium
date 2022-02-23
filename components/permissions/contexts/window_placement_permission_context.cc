// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/window_placement_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

WindowPlacementPermissionContext::WindowPlacementPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::WINDOW_PLACEMENT,
          blink::mojom::PermissionsPolicyFeature::kWindowPlacement) {}

WindowPlacementPermissionContext::~WindowPlacementPermissionContext() = default;

bool WindowPlacementPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

void WindowPlacementPermissionContext::UserMadePermissionDecision(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  // Notify user activation on the requesting frame if permission was granted,
  // as transient activation may have expired while the user was responding.
  // This enables sites to prompt for permission to access multi-screen info and
  // then immediately request fullscreen or place a window using granted info.
  if (content_setting == CONTENT_SETTING_ALLOW) {
    if (auto* render_frame_host = content::RenderFrameHost::FromID(
            id.render_process_id(), id.render_frame_id())) {
      render_frame_host->NotifyUserActivation(
          blink::mojom::UserActivationNotificationType::kInteraction);
    }
  }

  PermissionContextBase::UserMadePermissionDecision(
      id, requesting_origin, embedding_origin, content_setting);
}

}  // namespace permissions
