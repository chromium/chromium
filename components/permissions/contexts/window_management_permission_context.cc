// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/window_management_permission_context.h"

#include "base/feature_list.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

WindowManagementPermissionContext::WindowManagementPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::WINDOW_MANAGEMENT,
          blink::mojom::PermissionsPolicyFeature::kWindowManagement) {}

WindowManagementPermissionContext::~WindowManagementPermissionContext() =
    default;

#if BUILDFLAG(IS_ANDROID)
ContentSetting WindowManagementPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // TODO(crbug.com/40092782): Add window-management support on Android.
  NOTIMPLEMENTED_LOG_ONCE();
  return CONTENT_SETTING_BLOCK;
}
#endif  // IS_ANDROID

void WindowManagementPermissionContext::UserMadePermissionDecision(
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
            id.global_render_frame_host_id())) {
      render_frame_host->NotifyUserActivation(
          blink::mojom::UserActivationNotificationType::kInteraction);
    }
  }

  PermissionContextBase::UserMadePermissionDecision(
      id, requesting_origin, embedding_origin, content_setting);
}

}  // namespace permissions
