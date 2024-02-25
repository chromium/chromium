// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_session.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/disks/mount_point.h"
#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"

namespace drivefs {

namespace {

using MountFailure = DriveFsSession::MountObserver::MountFailure;
constexpr char kDataDirOption[] = "datadir=";
constexpr char kMyFilesOption[] = "myfiles=";
constexpr char kMountScheme[] = "drivefs://";
constexpr base::TimeDelta kMountTimeout = base::Seconds(20);

class DiskMounterImpl : public DiskMounter {
 public:
  explicit DiskMounterImpl(ash::disks::DiskMountManager* disk_mount_manager)
      : disk_mount_manager_(disk_mount_manager) {}

  DiskMounterImpl(const DiskMounterImpl&) = delete;
  DiskMounterImpl& operator=(const DiskMounterImpl&) = delete;

  ~DiskMounterImpl() override = default;

  void Mount(const base::UnguessableToken& token,
             const base::FilePath& data_path,
             const base::FilePath& my_files_path,
             const std::string& desired_mount_dir_name,
             base::OnceCallback<void(base::FilePath)> callback) override {
    DCHECK(!mount_point_);
    DCHECK(callback_.is_null());
    callback_ = std::move(callback);

    source_path_ = base::StrCat({kMountScheme, token.ToString()});
    std::string datadir_option =
        base::StrCat({kDataDirOption, data_path.value()});

    ash::disks::MountPoint::Mount(
        disk_mount_manager_, source_path_, "", desired_mount_dir_name,
        {datadir_option, base::StrCat({kMyFilesOption, my_files_path.value()})},
        ash::MountType::kNetworkStorage, ash::MountAccessMode::kReadWrite,
        base::BindOnce(&DiskMounterImpl::OnMountDone,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  // MountPoint::Mount() done callback.
  void OnMountDone(ash::MountError error_code,
                   std::unique_ptr<ash::disks::MountPoint> mount_point) {
    DCHECK(callback_);

    if (error_code != ash::MountError::kSuccess) {
      LOG(WARNING) << "DriveFs mount failed with error: " << error_code;
      std::move(callback_).Run({});
      return;
    }

    DCHECK(mount_point);
    mount_point_ = std::move(mount_point);
    std::move(callback_).Run(mount_point_->mount_path());
  }

  const raw_ptr<ash::disks::DiskMountManager> disk_mount_manager_;
  base::OnceCallback<void(base::FilePath)> callback_;
  // The path passed to cros-disks to mount.
  std::string source_path_;
  std::unique_ptr<ash::disks::MountPoint> mount_point_;

  base::WeakPtrFactory<DiskMounterImpl> weak_factory_{this};
};

}  // namespace

std::unique_ptr<DiskMounter> DiskMounter::Create(
    ash::disks::DiskMountManager* disk_mount_manager) {
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
    std::optional<base::TimeDelta> remount_delay) {
  // May delete |this|.
  auto connection = std::move(connection_);
  if (connection) {
    observer_->OnMountFailed(failure, remount_delay);
  }
}

void DriveFsSession::NotifyUnmounted(
    std::optional<base::TimeDelta> remount_delay) {
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
    std::optional<base::TimeDelta> remount_delay) {
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

void DriveFsSession::OnUnmounted(std::optional<base::TimeDelta> remount_delay) {
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
