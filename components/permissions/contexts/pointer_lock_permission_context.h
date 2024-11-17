// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_POINTER_LOCK_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_POINTER_LOCK_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class PointerLockPermissionContext : public permissions::PermissionContextBase {
 public:
  explicit PointerLockPermissionContext(
      content::BrowserContext* browser_context);
  ~PointerLockPermissionContext() override = default;

  PointerLockPermissionContext(const PointerLockPermissionContext&) = delete;
  PointerLockPermissionContext& operator=(const PointerLockPermissionContext&) =
      delete;

  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override;

#if !BUILDFLAG(IS_ANDROID)
 private:
  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
#endif
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_POINTER_LOCK_PERMISSION_CONTEXT_H_
