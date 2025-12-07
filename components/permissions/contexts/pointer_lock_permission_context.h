// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_POINTER_LOCK_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_POINTER_LOCK_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_decision.h"

namespace permissions {

// TODO(crbug.com/391651884): Remove this class since Pointer Lock Permission is
// not needed.
class PointerLockPermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  explicit PointerLockPermissionContext(
      content::BrowserContext* browser_context);
  ~PointerLockPermissionContext() override = default;

  PointerLockPermissionContext(const PointerLockPermissionContext&) = delete;
  PointerLockPermissionContext& operator=(const PointerLockPermissionContext&) =
      delete;

  void NotifyPermissionSet(const PermissionRequestData& request_data,
                           BrowserPermissionCallback callback,
                           bool persist,
                           PermissionDecision decision,
                           bool is_final_decision) override;

#if !BUILDFLAG(IS_ANDROID)
 private:
  // ContentSettingPermissionContextBase:
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
#endif
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_POINTER_LOCK_PERMISSION_CONTEXT_H_
