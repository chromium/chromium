// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/suspend_unmount_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace ash::disks {
namespace {

using base::TimeDelta;
using chromeos::PowerManagerClient;

}  // namespace

SuspendUnmountManager::SuspendUnmountManager(
    DiskMountManager* disk_mount_manager)
    : disk_mount_manager_(disk_mount_manager) {
  PowerManagerClient::Get()->AddObserver(this);
}

SuspendUnmountManager::~SuspendUnmountManager() {
  PowerManagerClient::Get()->RemoveObserver(this);
  if (block_suspend_token_) {
    PowerManagerClient::Get()->UnblockSuspend(block_suspend_token_);
    VLOG(1) << "Unblocked the suspension";
  }
}

void SuspendUnmountManager::SuspendImminent(
    const power_manager::SuspendImminent::Reason reason) {
  VLOG(1) << "SuspendImminent("
          << power_manager::SuspendImminent::Reason_Name(reason) << ")";

  // Start unmounting the removable devices.
  for (const std::unique_ptr<Disk>& disk : disk_mount_manager_->disks()) {
    const DeviceType t = disk->device_type();
    if (t != DeviceType::kUSB && t != DeviceType::kSD) {
      continue;
    }

    const std::string& path = disk->mount_path();
    if (path.empty()) {
      continue;
    }

    if (unmounting_paths_.insert(path).second) {
      VLOG(1) << "Unmounting '" << path << "'";
      disk_mount_manager_->UnmountPath(
          path, base::BindOnce(&SuspendUnmountManager::OnUnmountComplete,
                               weak_ptr_factory_.GetWeakPtr(), path));
    } else {
      VLOG(1) << "Already unmounting '" << path << "'";
    }
  }

  if (unmounting_paths_.empty()) {
    VLOG(1) << "No removable device to unmount before going to sleep";
    return;
  }

  VLOG(1) << "Unmounting " << unmounting_paths_.size()
          << " removable drives before going to sleep";

  if (block_suspend_token_.is_empty()) {
    block_suspend_token_ = base::UnguessableToken::Create();
    block_suspend_time_ = base::TimeTicks::Now();
    PowerManagerClient::Get()->BlockSuspend(block_suspend_token_,
                                            "SuspendUnmountManager");
    VLOG(1) << "Delaying the suspension";
  }
}

void SuspendUnmountManager::SuspendDone(const TimeDelta sleep_duration) {
  VLOG(1) << "SuspendDone(" << sleep_duration << ")";

  // SuspendDone can be called before OnUnmountComplete when suspend is
  // cancelled, or it takes a long time to unmount volumes.

  if (block_suspend_token_) {
    PowerManagerClient::Get()->UnblockSuspend(block_suspend_token_);
    block_suspend_token_ = {};
    VLOG(1) << "Unblocked the suspension";
  }

  if (unmounting_paths_.empty()) {
    VLOG(1) << "Remounting all the removable drives";
    disk_mount_manager_->EnsureMountInfoRefreshed(base::DoNothing(),
                                                  true /* force */);
  } else {
    LOG(WARNING) << "There are still " << unmounting_paths_.size()
                 << " removable drives waiting to be unmounted when the system"
                    " is waking up after sleeping for "
                 << sleep_duration;
    base::UmaHistogramSparse("CrosDisks.StillUnmountingWhenWakingUp",
                             unmounting_paths_.size());
  }
}

void SuspendUnmountManager::OnUnmountComplete(const std::string& mount_path,
                                              const MountError error_code) {
  VLOG(1) << "Unmounted '" << mount_path << "': " << error_code;

  const bool tracked = unmounting_paths_.erase(mount_path);
  DCHECK(tracked) << " Mount point '" << mount_path << "' is not tracked";

  if (!unmounting_paths_.empty()) {
    VLOG(1) << "Still waiting for " << unmounting_paths_.size()
            << " removable drives to be unmounted";
    return;
  }

  const TimeDelta block_time = base::TimeTicks::Now() - block_suspend_time_;
  VLOG(1) << "Unmounted all the removable drives in " << block_time;

  if (block_suspend_token_) {
    PowerManagerClient::Get()->UnblockSuspend(block_suspend_token_);
    block_suspend_token_ = {};
    VLOG(1) << "Unblocked the suspension after " << block_time;
    base::UmaHistogramMediumTimes("CrosDisks.Time.BlockSuspend", block_time);
  } else {
    VLOG(1) << "Remounting all the removable drives";
    disk_mount_manager_->EnsureMountInfoRefreshed(base::DoNothing(),
                                                  true /* force */);
  }
}

}  // namespace ash::disks
