// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_KEYBOARD_LOCK_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_KEYBOARD_LOCK_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class KeyboardLockPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit KeyboardLockPermissionContext(
      content::BrowserContext* browser_context);
  ~KeyboardLockPermissionContext() override = default;

  KeyboardLockPermissionContext(const KeyboardLockPermissionContext&) = delete;
  KeyboardLockPermissionContext& operator=(
      const KeyboardLockPermissionContext&) = delete;

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

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_KEYBOARD_LOCK_PERMISSION_CONTEXT_H_
