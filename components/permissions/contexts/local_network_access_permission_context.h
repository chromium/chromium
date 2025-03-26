// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class LocalNetworkAccessPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit LocalNetworkAccessPermissionContext(
      content::BrowserContext* browser_context);
  ~LocalNetworkAccessPermissionContext() override;

  LocalNetworkAccessPermissionContext(
      const LocalNetworkAccessPermissionContext&) = delete;
  LocalNetworkAccessPermissionContext& operator=(
      const LocalNetworkAccessPermissionContext&) = delete;
};

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_NETWORK_ACCESS_PERMISSION_CONTEXT_H_
