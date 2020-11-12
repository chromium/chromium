// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/webxr_permission_context.h"

#include "base/check.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#endif

namespace permissions {
WebXrPermissionContext::WebXrPermissionContext(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(browser_context,
                            content_settings_type,
                            blink::mojom::FeaturePolicyFeature::kWebXr),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::VR ||
         content_settings_type_ == ContentSettingsType::AR);
}

WebXrPermissionContext::~WebXrPermissionContext() = default;

bool WebXrPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

#if defined(OS_ANDROID)
// There are two other permissions that need to check corresponding OS-level
// permissions, and they take two different approaches to this. Geolocation only
// stores the permission ContentSetting if both requests are granted (or if the
// site permission is "Block"). The media permissions follow something more
// similar to this approach, first querying and storing the site-specific
// ContentSetting and then querying for the additional OS permissions as needed.
// However, this is done in MediaStreamDevicesController, not their permission
// context. By persisting and then running additional code as needed, we thus
// mimic that flow, but keep all logic contained into the permission context
// class.
void WebXrPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  // Only AR needs to check for additional permissions, and then only if it was
  // actually allowed.
  if (!(content_settings_type_ == ContentSettingsType::AR &&
        content_setting == ContentSetting::CONTENT_SETTING_ALLOW)) {
    PermissionContextBase::NotifyPermissionSet(
        id, requesting_origin, embedding_origin, std::move(callback), persist,
        content_setting);
    return;
  }

  // Whether or not the user will ultimately accept the OS permissions, we want
  // to save the content_setting here if we should.
  if (persist) {
    PermissionContextBase::UpdateContentSetting(
        requesting_origin, embedding_origin, content_setting);
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.render_process_id(),
                                           id.render_frame_id()));
  if (!web_contents) {
    // If we can't get the web contents, we don't know the state of the OS
    // permission, so assume we don't have it.
    OnAndroidPermissionDecided(id, requesting_origin, embedding_origin,
                               std::move(callback),
                               false /*permission_granted*/);
    return;
  }

  // Otherwise, the user granted permission to use AR, so now we need to check
  // if we need to prompt for android system permissions.
  std::vector<ContentSettingsType> permission_type = {content_settings_type_};
  PermissionRepromptState reprompt_state =
      ShouldRepromptUserForPermissions(web_contents, permission_type);
  switch (reprompt_state) {
    case PermissionRepromptState::kNoNeed:
      // We have already returned if permission was denied by the user, and this
      // indicates that we have all the OS permissions we need.
      OnAndroidPermissionDecided(id, requesting_origin, embedding_origin,
                                 std::move(callback),
                                 true /*permission_granted*/);
      return;

    case PermissionRepromptState::kCannotShow:
      // If we cannot show the info bar, then we have to assume we don't have
      // the permissions we need.
      OnAndroidPermissionDecided(id, requesting_origin, embedding_origin,
                                 std::move(callback),
                                 false /*permission_granted*/);
      return;

    case PermissionRepromptState::kShow:
      // Otherwise, prompt the user that we need additional permissions.
      PermissionsClient::Get()->RepromptForAndroidPermissions(
          web_contents, permission_type,
          base::BindOnce(&WebXrPermissionContext::OnAndroidPermissionDecided,
                         weak_ptr_factory_.GetWeakPtr(), id, requesting_origin,
                         embedding_origin, std::move(callback)));
      return;
  }
}

void WebXrPermissionContext::OnAndroidPermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool permission_granted) {
  // If we were supposed to persist the setting we've already done so in the
  // initial override of |NotifyPermissionSet|. At this point, if the user
  // has denied the OS level permission, we want to notify the requestor that
  // the permission has been blocked.
  // TODO(https://crbug.com/1060163): Ensure that this is taken into account
  // when returning navigator.permissions results.
  ContentSetting setting = permission_granted
                               ? ContentSetting::CONTENT_SETTING_ALLOW
                               : ContentSetting::CONTENT_SETTING_BLOCK;
  PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback),
      false /*persist*/, setting);
}
#endif  // defined(OS_ANDROID)
}  // namespace permissions
