// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_AUTOMATIC_FULLSCREEN_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_AUTOMATIC_FULLSCREEN_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class AutomaticFullscreenPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit AutomaticFullscreenPermissionContext(
      content::BrowserContext* browser_context);
  ~AutomaticFullscreenPermissionContext() override = default;

  AutomaticFullscreenPermissionContext(
      const AutomaticFullscreenPermissionContext&) = delete;
  AutomaticFullscreenPermissionContext& operator=(
      const AutomaticFullscreenPermissionContext&) = delete;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_AUTOMATIC_FULLSCREEN_PERMISSION_CONTEXT_H_
