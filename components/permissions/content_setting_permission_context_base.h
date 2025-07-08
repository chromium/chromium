// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTENT_SETTING_PERMISSION_CONTEXT_BASE_H_
#define COMPONENTS_PERMISSIONS_CONTENT_SETTING_PERMISSION_CONTEXT_BASE_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

// This context base class implements common functionality shared by permissions
// that fulfill the restriction that their permission state can be fully
// represented with a `ContentSetting` value. This means they have no need to
// support multi-state information such as permission options or chooser data.
// This allows them to be handled in a standardized way, reducing the complexity
// of their implementation.
class ContentSettingPermissionContextBase : public PermissionContextBase {
 public:
  explicit ContentSettingPermissionContextBase(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      network::mojom::PermissionsPolicyFeature permissions_policy_feature);

  // Resets the content setting value
  void ResetPermission(const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

 protected:
  // Store the decided permission state. Virtual since the permission might be
  // stored with different restrictions (for example for desktop notifications).
  virtual void UpdateContentSetting(const PermissionRequestData& request_data,
                                    ContentSetting content_setting,
                                    bool is_one_time);

  // Retrieve the current permission state. Virtual since the permission might
  // want to customize logic around retrieving permission states.
  virtual ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const;

 private:
  // The private final methods below are used to hide base::Value from
  // subclasses. Override the corresponding protected virtual methods to
  // override permission-specific functionality.

  // PermissionContextBase:
  PermissionSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const final;

  // PermissionContextBase:
  void UpdateSetting(const PermissionRequestData& request_data,
                     PermissionSetting content_setting,
                     bool is_one_time) final;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTENT_SETTING_PERMISSION_CONTEXT_BASE_H_
