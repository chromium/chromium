// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_

#include "base/component_export.h"

namespace drivefs::pinning {

// Manages bulk pinning of items via DriveFS. This class handles the following:
//  - Manage batching of pin actions to avoid sending too many events at once.
//  - Ensure disk space is not being exceeded whilst pinning files.
//  - Maintain pinning of files that are newly created.
//  - Rebuild the progress of bulk pinned items (if turned off mid way through a
//    bulk pinning event).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsPinManager {
 public:
  explicit DriveFsPinManager(bool enabled);

  DriveFsPinManager(const DriveFsPinManager&) = delete;
  DriveFsPinManager& operator=(const DriveFsPinManager&) = delete;

  ~DriveFsPinManager() = default;

  // Enable or disable the bulk pinning.
  void SetBulkPinningEnabled(bool enabled) { enabled_ = enabled; }

 private:
  bool enabled_ = false;
};

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_