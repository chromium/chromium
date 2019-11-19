// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_session.h"

#include <utility>

#include "base/strings/strcat.h"
#include "chromeos/components/drivefs/drivefs_bootstrap.h"

namespace drivefs {

namespace {

using MountFailure = DriveFsSession::MountObserver::MountFailure;
constexpr char kDataDirOption[] = "datadir=";
constexpr char kMyFilesOption[] = "myfiles=";
constexpr char kMountScheme[] = "drivefs://";
constexpr base::TimeDelta kMountTimeout = base::TimeDelta::FromSeconds(20);

class DiskMounterImpl : public DiskMounter,
                        public chromeos::disks::DiskMountManager::Observer {
 public:
  explicit DiskMounterImpl(
      chromeos::disks::DiskMountManager* disk_mount_manager)
      : disk_mount_manager_(disk_mount_manager) {}

  ~DiskMounterImpl() override {
    disk_mount_manager_->RemoveObserver(this);
    if (!mount_path_.empty()) {
      Unmount();
    }
  }

  void Mount(const base::UnguessableToken& token,
             const base::FilePath& data_path,
             const base::FilePath& my_files_path,
             const std::string& desired_mount_dir_name,
             base::OnceCallback<void(base::FilePath)> callback) override {
    DCHECK(mount_path_.empty());
    DCHECK(callback_.is_null());
    callback_ = std::move(callback);

    disk_mount_manager_->AddObserver(this);
    source_path_ = base::StrCat({kMountScheme, token.ToString()});
    std::string datadir_option =
        base::StrCat({kDataDirOption, data_path.value()});
    disk_mount_manager_->MountPath(
        source_path_, "", desired_mount_dir_name,
        {datadir_option, base::StrCat({kMyFilesOption, my_files_path.value()})},
        chromeos::MOUNT_TYPE_NETWORK_STORAGE,
        chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  }

 private:
  void Unmount() {
    disk_mount_manager_->RemoveObserver(this);
    if (!mount_path_.empty()) {
      disk_mount_manager_->UnmountPath(mount_path_.value(), {});
      mount_path_.clear();
    }
  }

  // DiskMountManager::Observer:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override {
    if (mount_info.mount_type != chromeos::MOUNT_TYPE_NETWORK_STORAGE ||
        mount_info.source_path != source_path_ ||
        event != chromeos::disks::DiskMountManager::MOUNTING) {
      return;
    }
    DCHECK(mount_path_.empty());
    DCHECK(!callback_.is_null());
    if (error_code == chromeos::MOUNT_ERROR_NONE) {
      disk_mount_manager_->RemoveObserver(this);
      DCHECK(!mount_info.mount_path.empty());
      mount_path_ = base::FilePath(mount_info.mount_path);
    }
    std::move(callback_).Run(mount_path_);
  }

  chromeos::disks::DiskMountManager* const disk_mount_manager_;
  base::OnceCallback<void(base::FilePath)> callback_;
  // The path passed to cros-disks to mount.
  std::string source_path_;
  base::FilePath mount_path_;

  DISALLOW_COPY_AND_ASSIGN(DiskMounterImpl);
};

}  // namespace

std::unique_ptr<DiskMounter> DiskMounter::Create(
    chromeos::disks::DiskMountManager* disk_mount_manager) {
  return std::make_unique<DiskMounterImpl>(disk_mount_manager);
}

DriveFsSession::DriveFsSession(base::OneShotTimer* timer,
                               std::unique_ptr<DiskMounter> disk_mounter,
                               std::unique_ptr<DriveFsConnection> connection,
                               const base::FilePath& data_path,
                               const base::FilePath& my_files_path,
                               const std::string& desired_mount_dir_name,
                               MountObserver* observer)
    : timer_(timer),
      disk_mounter_(std::move(disk_mounter)),
      connection_(std::move(connection)),
      observer_(observer) {
  auto token = connection_->Connect(
      this, base::BindOnce(&DriveFsSession::OnMojoConnectionError,
                           base::Unretained(this)));
  CHECK(token);
  drivefs_ = &connection_->GetDriveFs();
  disk_mounter_->Mount(token, data_path, my_files_path, desired_mount_dir_name,
                       base::BindOnce(&DriveFsSession::OnDiskMountCompleted,
                                      base::Unretained(this)));
  timer_->Start(
      FROM_HERE, kMountTimeout,
      base::BindOnce(&DriveFsSession::OnMountTimedOut, base::Unretained(this)));
}

DriveFsSession::~DriveFsSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_->Stop();
  is_mounted_ = false;
  drivefs_has_terminated_ = true;
  disk_mounter_.reset();
}

void DriveFsSession::OnDiskMountCompleted(base::FilePath mount_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!mount_path.empty()) {
    mount_path_ = mount_path;
    MaybeNotifyOnMounted();
  } else {
    NotifyFailed(MountFailure::kInvocation, {});
  }
}

void DriveFsSession::OnMojoConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!drivefs_has_terminated_) {
    bool was_mounted = is_mounted_;
    is_mounted_ = false;
    drivefs_has_terminated_ = true;
    if (was_mounted) {
      NotifyUnmounted({});
    } else {
      NotifyFailed(MountFailure::kIpcDisconnect, {});
    }
  }
}

void DriveFsSession::OnMountTimedOut() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_mounted_);
  DCHECK(!drivefs_has_terminated_);
  timer_->Stop();
  drivefs_has_terminated_ = true;
  NotifyFailed(MountFailure::kTimeout, {});
}

void DriveFsSession::MaybeNotifyOnMounted() {
  is_mounted_ =
      drivefs_has_started_ && !drivefs_has_terminated_ && !mount_path_.empty();
  if (is_mounted_) {
    timer_->Stop();
    observer_->OnMounted(mount_path());
  }
}

void DriveFsSession::NotifyFailed(
    MountFailure failure,
    base::Optional<base::TimeDelta> remount_delay) {
  // May delete |this|.
  auto connection = std::move(connection_);
  if (connection) {
    observer_->OnMountFailed(failure, remount_delay);
  }
}

void DriveFsSession::NotifyUnmounted(
    base::Optional<base::TimeDelta> remount_delay) {
  // May delete |this|.
  auto connection = std::move(connection_);
  if (connection) {
    observer_->OnUnmounted(remount_delay);
  }
}

void DriveFsSession::OnMounted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!drivefs_has_started_);
  DCHECK(!is_mounted_);
  if (!drivefs_has_terminated_) {
    drivefs_has_started_ = true;
    MaybeNotifyOnMounted();
  }
}

void DriveFsSession::OnMountFailed(
    base::Optional<base::TimeDelta> remount_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!drivefs_has_started_);
  DCHECK(!is_mounted_);
  if (!drivefs_has_terminated_) {
    drivefs_has_terminated_ = true;
    bool needs_restart = remount_delay.has_value();
    NotifyFailed(
        needs_restart ? MountFailure::kNeedsRestart : MountFailure::kUnknown,
        std::move(remount_delay));
  }
}

void DriveFsSession::OnUnmounted(
    base::Optional<base::TimeDelta> remount_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(drivefs_has_started_);
  DCHECK(!drivefs_has_terminated_);
  DCHECK(is_mounted_);
  bool was_mounted = is_mounted_;
  drivefs_has_terminated_ = true;
  is_mounted_ = false;
  if (was_mounted) {
    NotifyUnmounted(std::move(remount_delay));
  }
}

void DriveFsSession::OnHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_->IsRunning()) {
    timer_->Reset();
  }
}

}  // namespace drivefs
