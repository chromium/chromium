// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
#define COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

// Manages user permissions for background sync. The context is scoped to the
// requesting origin, which should always be equal to the top-level origin as
// background syncs can only be requested from top-level pages.
// The permission status is ALLOW by default and can be changed globally or on a
// per-site basis from the content settings page. The user is not prompted for
// permission.
class BackgroundSyncPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit BackgroundSyncPermissionContext(
      content::BrowserContext* browser_context);

  BackgroundSyncPermissionContext(const BackgroundSyncPermissionContext&) =
      delete;
  BackgroundSyncPermissionContext& operator=(
      const BackgroundSyncPermissionContext&) = delete;

  ~BackgroundSyncPermissionContext() override = default;

 private:
  // PermissionContextBase:
  void DecidePermission(
      permissions::PermissionRequestData request_Data,
      permissions::BrowserPermissionCallback callback) override;
};

#endif  // COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
