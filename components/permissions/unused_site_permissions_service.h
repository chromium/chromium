// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_H_
#define COMPONENTS_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace permissions {

// Add explanation
class UnusedSitePermissionsService : public KeyedService {
 public:
  explicit UnusedSitePermissionsService(content::BrowserContext* context);

  UnusedSitePermissionsService(const UnusedSitePermissionsService&) = delete;
  UnusedSitePermissionsService& operator=(const UnusedSitePermissionsService&) =
      delete;

  ~UnusedSitePermissionsService() override;

  // KeyedService implementation.
  void Shutdown() override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_H_