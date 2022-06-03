// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WINDOW_PLACEMENT_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WINDOW_PLACEMENT_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class WindowPlacementPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit WindowPlacementPermissionContext(
      content::BrowserContext* browser_context);
  ~WindowPlacementPermissionContext() override;

  WindowPlacementPermissionContext(const WindowPlacementPermissionContext&) =
      delete;
  WindowPlacementPermissionContext& operator=(
      const WindowPlacementPermissionContext&) = delete;

 protected:
  // permissions::PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;
  void UserMadePermissionDecision(const permissions::PermissionRequestID& id,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  ContentSetting content_setting) override;
};

#endif  // CHROME_BROWSER_WINDOW_PLACEMENT_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_
