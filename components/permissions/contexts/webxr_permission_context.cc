// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/webxr_permission_context.h"

#include "base/check.h"
#include "build/build_config.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/android/permissions_reprompt_controller_android.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#if BUILDFLAG(ENABLE_VR)
#include "base/feature_list.h"
#include "device/vr/public/cpp/features.h"
#endif
#endif

namespace permissions {
WebXrPermissionContext::WebXrPermissionContext(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(browser_context,
                            content_settings_type,
                            blink::mojom::PermissionsPolicyFeature::kWebXr),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::VR ||
         content_settings_type_ == ContentSettingsType::AR ||
         content_settings_type_ == ContentSettingsType::HAND_TRACKING);
}

WebXrPermissionContext::~WebXrPermissionContext() = default;

#if BUILDFLAG(IS_ANDROID)
// There are two other permissions that need to check corresponding OS-level
// permissions, and they take two different approaches to this. Geolocation only
// stores the permission ContentSetting if both requests are granted (or if the
// site permission is "Block"). The media permissions are now following the
// approach found here.
void WebXrPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  const bool is_hands =
      content_settings_type_ == ContentSettingsType::HAND_TRACKING;
  DCHECK(!is_one_time || is_hands);
  DCHECK(is_final_decision);

  // Note that this method calls into base class implementation version of
  // `NotifyPermissionSet()`, which would call `UpdateTabContext()`.
  // This is fine, even in cases where we call the base method with a parameter
  // that does not correspond to user's answer to Chrome-level permission,
  // because `WebXrPermissionContext` does *not* have a custom implementation
  // for `UpdateTabContext()` - if it did, we'd need to stop calling into base
  // class with the parameter not matching user's answer.

  // If permission was denied, we don't need to check for additional
  // permissions. We also don't need to check for additional permissions for
  // non-OpenXR VR.
  const bool permission_granted =
      content_setting == ContentSetting::CONTENT_SETTING_ALLOW;
  const bool is_ar = content_settings_type_ == ContentSettingsType::AR;
  bool is_openxr = false;
#if BUILDFLAG(ENABLE_OPENXR)
  is_openxr = content_settings_type_ == ContentSettingsType::VR &&
              device::features::IsOpenXrEnabled();
#endif
  const bool additional_permissions_needed =
      permission_granted && (is_ar || is_openxr || is_hands);
  if (!additional_permissions_needed) {
    PermissionContextBase::NotifyPermissionSet(
        id, requesting_origin, embedding_origin, std::move(callback), persist,
        content_setting, is_one_time, is_final_decision);
    return;
  }

  // Whether or not the user will ultimately accept the OS permissions, we want
  // to save the content_setting here if we should.
  if (persist) {
    PermissionContextBase::UpdateContentSetting(
        requesting_origin, embedding_origin, content_setting, is_one_time);
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.global_render_frame_host_id()));
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
      permissions::PermissionsRepromptControllerAndroid::CreateForWebContents(
          web_contents);
      permissions::PermissionsRepromptControllerAndroid::FromWebContents(
          web_contents)
          ->RepromptPermissionRequest(
              permission_type, content_settings_type(),
              base::BindOnce(
                  &WebXrPermissionContext::OnAndroidPermissionDecided,
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
  // TODO(crbug.com/40678885): Ensure that this is taken into account
  // when returning navigator.permissions results.
  ContentSetting setting = permission_granted
                               ? ContentSetting::CONTENT_SETTING_ALLOW
                               : ContentSetting::CONTENT_SETTING_BLOCK;
  PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback),
      false /*persist*/, setting, /*is_one_time=*/false,
      /*is_final_decision=*/true);
}

void WebXrPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    bool allowed) {
  // See the comment in `NotifyPermissionSet()` for context on why this method
  // should be empty.
}

#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace permissions
