// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_MANAGEMENT_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_MANAGEMENT_PERMISSION_CONTEXT_H_

#include "build/build_config.h"
#include "components/permissions/content_setting_permission_context_base.h"

namespace permissions {

class WindowManagementPermissionContext
    : public ContentSettingPermissionContextBase {
 public:
  explicit WindowManagementPermissionContext(
      content::BrowserContext* browser_context);
  ~WindowManagementPermissionContext() override;

  WindowManagementPermissionContext(const WindowManagementPermissionContext&) =
      delete;
  WindowManagementPermissionContext& operator=(
      const WindowManagementPermissionContext&) = delete;

 protected:
  // ContentSettingPermissionContextBase:
#if BUILDFLAG(IS_ANDROID)
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
#endif  // IS_ANDROID
  void UserMadePermissionDecision(const PermissionRequestID& id,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  PermissionDecision decision) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_MANAGEMENT_PERMISSION_CONTEXT_H_
