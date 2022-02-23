// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class WindowPlacementPermissionContext : public PermissionContextBase {
 public:
  explicit WindowPlacementPermissionContext(
      content::BrowserContext* browser_context);
  ~WindowPlacementPermissionContext() override;

  WindowPlacementPermissionContext(const WindowPlacementPermissionContext&) =
      delete;
  WindowPlacementPermissionContext& operator=(
      const WindowPlacementPermissionContext&) = delete;

 protected:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;
  void UserMadePermissionDecision(const PermissionRequestID& id,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  ContentSetting content_setting) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_
