// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DISKS_SUSPEND_UNMOUNT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DISKS_SUSPEND_UNMOUNT_MANAGER_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {
namespace disks {

class DiskMountManager;

// Class to unmount disks at suspend.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DISKS) SuspendUnmountManager
    : public chromeos::PowerManagerClient::Observer {
 public:
  // The ownership of these raw pointers still remains with the caller.
  explicit SuspendUnmountManager(DiskMountManager* disk_mount_manager);

  SuspendUnmountManager(const SuspendUnmountManager&) = delete;
  SuspendUnmountManager& operator=(const SuspendUnmountManager&) = delete;

  ~SuspendUnmountManager() override;

 private:
  void OnUnmountComplete(const std::string& mount_path, MountError error_code);

  // PowerManagerClient::Observer
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Callback passed to DiskMountManager holds weak pointers of this.
  const raw_ptr<DiskMountManager> disk_mount_manager_;

  // The paths that the manager currently tries to unmount for suspend.
  std::set<std::string> unmounting_paths_;

  base::UnguessableToken block_suspend_token_;
  base::TimeTicks block_suspend_time_;

  base::WeakPtrFactory<SuspendUnmountManager> weak_ptr_factory_{this};
};

}  // namespace disks
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DISKS_SUSPEND_UNMOUNT_MANAGER_H_
