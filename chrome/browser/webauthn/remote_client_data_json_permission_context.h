// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_REMOTE_CLIENT_DATA_JSON_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBAUTHN_REMOTE_CLIENT_DATA_JSON_PERMISSION_CONTEXT_H_

#include <memory>

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_context_base.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace permissions {
struct PermissionRequestData;
}  // namespace permissions

class GURL;

// Permission context for the WebAuthn `remoteClientDataJSON` extension,
// which is designated as a "powerful feature."
// https://w3c.github.io/webauthn/#sctn-remote-client-data-json-extension
//
// The permission state is derived from the
// `WebAuthenticationRemoteDesktopAllowedOrigins` and
// `WebAuthenticationRemoteProxiedRequestsAllowed` enterprise policies, surfaced
// via `OriginMayUseRemoteDesktopClientOverride()`. This context never prompts
// the user; it returns ALLOW or BLOCK based on policy only.
class RemoteClientDataJsonPermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  explicit RemoteClientDataJsonPermissionContext(
      content::BrowserContext* browser_context);

  RemoteClientDataJsonPermissionContext(
      const RemoteClientDataJsonPermissionContext&) = delete;
  RemoteClientDataJsonPermissionContext& operator=(
      const RemoteClientDataJsonPermissionContext&) = delete;

  ~RemoteClientDataJsonPermissionContext() override;

 protected:
  // ContentSettingPermissionContextBase:
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
  // PermissionContextBase:
  void DecidePermission(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback) override;

  void OnAuthorizationPolicyChanged();

  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_REMOTE_CLIENT_DATA_JSON_PERMISSION_CONTEXT_H_
