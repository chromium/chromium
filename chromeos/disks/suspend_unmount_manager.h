// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_SUSPEND_UNMOUNT_MANAGER_H_
#define CHROMEOS_DISKS_SUSPEND_UNMOUNT_MANAGER_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace chromeos {
namespace disks {

class DiskMountManager;

// Class to unmount disks at suspend.
class COMPONENT_EXPORT(CHROMEOS_DISKS) SuspendUnmountManager
    : public PowerManagerClient::Observer {
 public:
  // The ownership of these raw pointers still remains with the caller.
  explicit SuspendUnmountManager(DiskMountManager* disk_mount_manager);
  ~SuspendUnmountManager() override;

 private:
  void OnUnmountComplete(const std::string& mount_path,
                         chromeos::MountError error_code);

  // PowerManagerClient::Observer
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Callback passed to DiskMountManager holds weak pointers of this.
  DiskMountManager* const disk_mount_manager_;

  // The paths that the manager currently tries to unmount for suspend.
  std::set<std::string> unmounting_paths_;

  base::UnguessableToken block_suspend_token_;

  base::WeakPtrFactory<SuspendUnmountManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SuspendUnmountManager);
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_SUSPEND_UNMOUNT_MANAGER_H_
