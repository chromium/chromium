// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_SESSION_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace drivefs {

// Utility class to simplify mounting with DiskMountManager.
class COMPONENT_EXPORT(DRIVEFS) DiskMounter {
 public:
  DiskMounter() = default;
  virtual ~DiskMounter() = default;
  virtual void Mount(const base::UnguessableToken& token,
                     const base::FilePath& data_path,
                     const base::FilePath& my_files_path,
                     const std::string& desired_mount_dir_name,
                     base::OnceCallback<void(base::FilePath)> callback) = 0;

  static std::unique_ptr<DiskMounter> Create(
      chromeos::disks::DiskMountManager* disk_mount_manager);

 private:
  DISALLOW_COPY_AND_ASSIGN(DiskMounter);
};

class DriveFsConnection;

// Represents single Drive mount session. Hides the complexity
// of determining whether DriveFs is mounted or not.
class COMPONENT_EXPORT(DRIVEFS) DriveFsSession : public mojom::DriveFsDelegate {
 public:
  class MountObserver {
   public:
    enum class MountFailure {
      kUnknown,
      kNeedsRestart,
      kIpcDisconnect,
      kInvocation,
      kTimeout,
    };

    MountObserver() = default;
    virtual ~MountObserver() = default;
    virtual void OnMounted(const base::FilePath& mount_path) = 0;
    virtual void OnUnmounted(base::Optional<base::TimeDelta> remount_delay) = 0;
    virtual void OnMountFailed(
        MountFailure failure,
        base::Optional<base::TimeDelta> remount_delay) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(MountObserver);
  };

  DriveFsSession(base::OneShotTimer* timer,
                 std::unique_ptr<DiskMounter> disk_mounter,
                 std::unique_ptr<DriveFsConnection> connection,
                 const base::FilePath& data_path,
                 const base::FilePath& my_files_path,
                 const std::string& desired_mount_dir_name,
                 MountObserver* observer);
  ~DriveFsSession() override;

  // Returns whether DriveFS is mounted.
  bool is_mounted() const { return is_mounted_; }

  // Returns the path where DriveFS is mounted.
  const base::FilePath& mount_path() const { return mount_path_; }

  mojom::DriveFs* drivefs_interface() { return drivefs_; }

 private:
  // mojom::DriveFsDelegate:
  void OnMounted() final;
  void OnMountFailed(base::Optional<base::TimeDelta> remount_delay) final;
  void OnUnmounted(base::Optional<base::TimeDelta> remount_delay) final;
  void OnHeartbeat() final;

  void OnDiskMountCompleted(base::FilePath mount_path);
  void OnMojoConnectionError();
  void OnMountTimedOut();
  void MaybeNotifyOnMounted();
  void NotifyFailed(MountObserver::MountFailure failure,
                    base::Optional<base::TimeDelta> remount_delay);
  void NotifyUnmounted(base::Optional<base::TimeDelta> remount_delay);

  SEQUENCE_CHECKER(sequence_checker_);

  base::OneShotTimer* const timer_;
  std::unique_ptr<DiskMounter> disk_mounter_;
  std::unique_ptr<DriveFsConnection> connection_;
  MountObserver* const observer_;

  // The path where DriveFS is mounted.
  base::FilePath mount_path_;

  // Mojo interface to the DriveFS process.
  mojom::DriveFs* drivefs_ = nullptr;

  bool drivefs_has_started_ = false;
  bool drivefs_has_terminated_ = false;
  bool is_mounted_ = false;

  DISALLOW_COPY_AND_ASSIGN(DriveFsSession);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_SESSION_H_
