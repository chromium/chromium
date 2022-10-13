// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_SENSOR_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_SENSOR_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class SensorPermissionContext : public PermissionContextBase {
 public:
  explicit SensorPermissionContext(content::BrowserContext* browser_context);
  SensorPermissionContext(const SensorPermissionContext&) = delete;
  SensorPermissionContext& operator=(const SensorPermissionContext&) = delete;
  ~SensorPermissionContext() override;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_SENSOR_PERMISSION_CONTEXT_H_
