// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_ACCESSIBILITY_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_ACCESSIBILITY_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

// Manages permissions to expose accessibility events from assistive input
// devices. Generally websites do not know what kind of devices a user is using.
// This permission gives users the choice to disclose this information.
class AccessibilityPermissionContext : public PermissionContextBase {
 public:
  explicit AccessibilityPermissionContext(
      content::BrowserContext* browser_context);
  AccessibilityPermissionContext(const AccessibilityPermissionContext&) =
      delete;
  AccessibilityPermissionContext& operator=(
      const AccessibilityPermissionContext&) = delete;
  ~AccessibilityPermissionContext() override;

 private:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_ACCESSIBILITY_PERMISSION_CONTEXT_H_
