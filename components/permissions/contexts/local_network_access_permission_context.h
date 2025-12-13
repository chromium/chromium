// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"

namespace content {
class BrowserContext;
}

namespace permissions {

class LocalNetworkAccessPermissionContext
    : public ContentSettingPermissionContextBase {
 public:
  explicit LocalNetworkAccessPermissionContext(
      content::BrowserContext* browser_context);
  ~LocalNetworkAccessPermissionContext() override;

  LocalNetworkAccessPermissionContext(
      const LocalNetworkAccessPermissionContext&) = delete;
  LocalNetworkAccessPermissionContext& operator=(
      const LocalNetworkAccessPermissionContext&) = delete;

  bool IsRestrictedToSecureOrigins() const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_PERMISSION_CONTEXT_H_
