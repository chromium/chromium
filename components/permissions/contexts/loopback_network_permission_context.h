// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_LOOPBACK_NETWORK_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_LOOPBACK_NETWORK_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"

namespace content {
class BrowserContext;
}

namespace permissions {

class LoopbackNetworkPermissionContext
    : public ContentSettingPermissionContextBase {
 public:
  explicit LoopbackNetworkPermissionContext(
      content::BrowserContext* browser_context);
  ~LoopbackNetworkPermissionContext() override;

  LoopbackNetworkPermissionContext(const LoopbackNetworkPermissionContext&) =
      delete;
  LoopbackNetworkPermissionContext& operator=(
      const LoopbackNetworkPermissionContext&) = delete;

  bool IsRestrictedToSecureOrigins() const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_LOOPBACK_NETWORK_PERMISSION_CONTEXT_H_
