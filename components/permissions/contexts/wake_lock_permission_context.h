// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WAKE_LOCK_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WAKE_LOCK_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_context_base.h"

namespace permissions {

class WakeLockPermissionContext : public PermissionContextBase {
 public:
  WakeLockPermissionContext(content::BrowserContext* browser_context,
                            ContentSettingsType content_settings_type);
  WakeLockPermissionContext(const WakeLockPermissionContext&) = delete;
  WakeLockPermissionContext& operator=(const WakeLockPermissionContext&) =
      delete;
  ~WakeLockPermissionContext() override;

 private:
  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WAKE_LOCK_PERMISSION_CONTEXT_H_
