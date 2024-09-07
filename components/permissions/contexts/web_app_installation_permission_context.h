// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WEB_APP_INSTALLATION_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WEB_APP_INSTALLATION_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class WebAppInstallationPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit WebAppInstallationPermissionContext(
      content::BrowserContext* browser_context);
  ~WebAppInstallationPermissionContext() override = default;

  WebAppInstallationPermissionContext(
      const WebAppInstallationPermissionContext&) = delete;
  WebAppInstallationPermissionContext& operator=(
      const WebAppInstallationPermissionContext&) = delete;
};
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WEB_APP_INSTALLATION_PERMISSION_CONTEXT_H_
