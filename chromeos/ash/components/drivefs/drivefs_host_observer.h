// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_

#include <vector>

namespace drivefs {
namespace mojom {
class DriveError;
class FileChange;
class SyncingStatus;
}  // namespace mojom

class DriveFsHostObserver {
 public:
  virtual void OnUnmounted() {}
  virtual void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) {}
  virtual void OnMirrorSyncingStatusUpdate(const mojom::SyncingStatus& status) {
  }
  virtual void OnFilesChanged(const std::vector<mojom::FileChange>& changes) {}
  virtual void OnError(const mojom::DriveError& error) {}

 protected:
  virtual ~DriveFsHostObserver() = default;
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_
