// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/suspend_unmount_manager.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace ash {
namespace disks {
namespace {

// Threshold for logging the blocking of suspend.
constexpr base::TimeDelta kBlockSuspendThreshold = base::Seconds(5);

void OnRefreshCompleted(bool success) {}

}  // namespace

SuspendUnmountManager::SuspendUnmountManager(
    DiskMountManager* disk_mount_manager)
    : disk_mount_manager_(disk_mount_manager) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

SuspendUnmountManager::~SuspendUnmountManager() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  if (block_suspend_token_)
    chromeos::PowerManagerClient::Get()->UnblockSuspend(block_suspend_token_);
}

void SuspendUnmountManager::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  DCHECK(unmounting_paths_.empty());
  if (!unmounting_paths_.empty())
    return;
  std::set<std::string> mount_paths;
  for (const auto& disk : disk_mount_manager_->disks()) {
    if ((disk->device_type() == DeviceType::kUSB ||
         disk->device_type() == DeviceType::kSD) &&
        !disk->mount_path().empty()) {
      mount_paths.insert(disk->mount_path());
    }
  }
  for (const auto& mount_path : mount_paths) {
    if (block_suspend_token_.is_empty()) {
      block_suspend_token_ = base::UnguessableToken::Create();
      block_suspend_time_ = base::TimeTicks::Now();
      chromeos::PowerManagerClient::Get()->BlockSuspend(
          block_suspend_token_, "SuspendUnmountManager");
    }
    disk_mount_manager_->UnmountPath(
        mount_path, base::BindOnce(&SuspendUnmountManager::OnUnmountComplete,
                                   weak_ptr_factory_.GetWeakPtr(), mount_path));
    unmounting_paths_.insert(mount_path);
  }
}

void SuspendUnmountManager::SuspendDone(base::TimeDelta sleep_duration) {
  // SuspendDone can be called before OnUnmountComplete when suspend is
  // cancelled, or it takes long time to unmount volumes.
  unmounting_paths_.clear();
  disk_mount_manager_->EnsureMountInfoRefreshed(
      base::BindOnce(&OnRefreshCompleted), true /* force */);
  block_suspend_token_ = {};
}

void SuspendUnmountManager::OnUnmountComplete(const std::string& mount_path,
                                              MountError error_code) {
  // This can happen when unmount completes after suspend done is called.
  if (unmounting_paths_.erase(mount_path) != 1)
    return;
  if (unmounting_paths_.empty() && block_suspend_token_) {
    chromeos::PowerManagerClient::Get()->UnblockSuspend(block_suspend_token_);
    block_suspend_token_ = {};

    auto block_time = base::TimeTicks::Now() - block_suspend_time_;
    LOG_IF(WARNING, block_time > kBlockSuspendThreshold)
        << "Blocked suspend for " << block_time.InSecondsF() << " seconds";
  }
}

}  // namespace disks
}  // namespace ash
