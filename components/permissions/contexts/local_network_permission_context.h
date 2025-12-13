// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"

namespace content {
class BrowserContext;
}

namespace permissions {

class LocalNetworkPermissionContext
    : public ContentSettingPermissionContextBase {
 public:
  explicit LocalNetworkPermissionContext(
      content::BrowserContext* browser_context);
  ~LocalNetworkPermissionContext() override;

  LocalNetworkPermissionContext(const LocalNetworkPermissionContext&) = delete;
  LocalNetworkPermissionContext& operator=(
      const LocalNetworkPermissionContext&) = delete;

  bool IsRestrictedToSecureOrigins() const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_PERMISSION_CONTEXT_H_
