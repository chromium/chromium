// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_COMPAT_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_COMPAT_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class BrowserContext;
}

namespace permissions {

// This is used for the local-network-access permission when we have split
// permissions enabled. In that case, the local-network-access permission is an
// alias for the two permissions local-network and loopback-network, and running
// permission.query("local-network-access") gives results based on the value of
// those two permissions/content settings. The logic for this is in
// GetContentSettingStatusInternal().
class LocalNetworkAccessCompatPermissionContext
    : public ContentSettingPermissionContextBase {
 public:
  explicit LocalNetworkAccessCompatPermissionContext(
      content::BrowserContext* browser_context);
  ~LocalNetworkAccessCompatPermissionContext() override;

  LocalNetworkAccessCompatPermissionContext(
      const LocalNetworkAccessCompatPermissionContext&) = delete;
  LocalNetworkAccessCompatPermissionContext& operator=(
      const LocalNetworkAccessCompatPermissionContext&) = delete;

  void RequestPermission(std::unique_ptr<PermissionRequestData> request_data,
                         BrowserPermissionCallback callback) override;

 protected:
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  void UpdateContentSetting(
      const permissions::PermissionRequestData& request_data,
      ContentSetting content_setting,
      bool is_one_time) override;

  bool PermissionAllowedByPermissionsPolicy(
      content::RenderFrameHost* rfh) const override;

  void NotifyObservers(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsTypeSet content_type_set) const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_COMPAT_PERMISSION_CONTEXT_H_
