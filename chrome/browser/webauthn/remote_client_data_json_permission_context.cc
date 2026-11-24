// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/remote_client_data_json_permission_context.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/chrome_web_authentication_delegate_base.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_data.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_result.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

RemoteClientDataJsonPermissionContext::RemoteClientDataJsonPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON,
          network::mojom::PermissionsPolicyFeature::
              kPublicKeyCredentialsRemoteClientDataJson) {
  pref_change_registrar_.Init(
      Profile::FromBrowserContext(browser_context)->GetPrefs());
  pref_change_registrar_.Add(
      webauthn::pref_names::kRemoteDesktopAllowedOrigins,
      base::BindRepeating(
          &RemoteClientDataJsonPermissionContext::OnAuthorizationPolicyChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      webauthn::pref_names::kRemoteProxiedRequestsAllowed,
      base::BindRepeating(
          &RemoteClientDataJsonPermissionContext::OnAuthorizationPolicyChanged,
          base::Unretained(this)));
}

RemoteClientDataJsonPermissionContext::
    ~RemoteClientDataJsonPermissionContext() = default;

ContentSetting
RemoteClientDataJsonPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // The permission state is derived entirely from enterprise-policy
  // authorization. This context never prompts the user.
  //
  // `OriginMayUseRemoteDesktopClientOverride()` is declared `final` on
  // `ChromeWebAuthenticationDelegateBase` with a platform-agnostic
  // implementation, so a local instance suffices for the authorization
  // check (all other delegate methods are unused here).
  ChromeWebAuthenticationDelegateBase delegate;
  if (delegate.OriginMayUseRemoteDesktopClientOverride(
          browser_context(), url::Origin::Create(requesting_origin))) {
    return CONTENT_SETTING_ALLOW;
  }
  return CONTENT_SETTING_BLOCK;
}

void RemoteClientDataJsonPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  // The status is always ALLOW or BLOCK from policy, so a request is normally
  // resolved from it. A request only gets here when PermissionContextBase
  // treats a BLOCK status as ignorable, such as for privileged surfaces like
  // the New Tab Page. This permission is never prompted for, so deny it.
  std::move(callback).Run(
      content::PermissionResult(blink::mojom::PermissionStatus::DENIED,
                                content::PermissionStatusSource::UNSPECIFIED));
}

void RemoteClientDataJsonPermissionContext::OnAuthorizationPolicyChanged() {
  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsTypeSet(content_settings_type()));
}
