// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_H_

namespace segmentation_platform {

// DatabaseMaintenance is responsible for running all relevant database
// maintenance tasks such as purging old data and removing newly unnecessary
// data.
class DatabaseMaintenance {
 public:
  virtual ~DatabaseMaintenance() = default;

  // Kicks of executing all maintenance tasks asynchronously.
  virtual void ExecuteMaintenanceTasks() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_H_
