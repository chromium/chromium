// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chromeos/components/drivefs/drivefs_auth.h"
#include "chromeos/components/drivefs/drivefs_session.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/account_id/account_id.h"

namespace drive {
class DriveNotificationManager;
}

namespace chromeos {
namespace disks {
class DiskMountManager;
}
}  // namespace chromeos

namespace network {
class NetworkConnectionTracker;
}

namespace drivefs {

class DriveFsBootstrapListener;
class DriveFsHostObserver;

// A host for a DriveFS process. In addition to managing its lifetime via
// mounting and unmounting, it also bridges between the DriveFS process and the
// file manager.
class COMPONENT_EXPORT(DRIVEFS) DriveFsHost {
 public:
  using MountObserver = DriveFsSession::MountObserver;

  class Delegate : public DriveFsAuth::Delegate {
   public:
    Delegate() = default;
    ~Delegate() override = default;

    virtual drive::DriveNotificationManager& GetDriveNotificationManager() = 0;
    virtual std::unique_ptr<DriveFsBootstrapListener> CreateMojoListener();
    virtual base::FilePath GetMyFilesPath() = 0;
    virtual std::string GetLostAndFoundDirectoryName() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  DriveFsHost(const base::FilePath& profile_path,
              Delegate* delegate,
              MountObserver* mount_observer,
              network::NetworkConnectionTracker* network_connection_tracker,
              const base::Clock* clock,
              chromeos::disks::DiskMountManager* disk_mount_manager,
              std::unique_ptr<base::OneShotTimer> timer);
  ~DriveFsHost();

  void AddObserver(DriveFsHostObserver* observer);
  void RemoveObserver(DriveFsHostObserver* observer);

  // Mount DriveFS.
  bool Mount();

  // Unmount DriveFS.
  void Unmount();

  // Returns whether DriveFS is mounted.
  bool IsMounted() const;

  // Returns the path where DriveFS is mounted.
  base::FilePath GetMountPath() const;

  // Returns the path where DriveFS keeps its data and caches.
  base::FilePath GetDataPath() const;

  mojom::DriveFs* GetDriveFsInterface() const;

  // Starts DriveFs search query and returns whether it will be
  // performed localy or remotely. Assumes DriveFS to be mounted.
  mojom::QueryParameters::QuerySource PerformSearch(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback);

 private:
  class AccountTokenDelegate;
  class MountState;

  std::string GetDefaultMountDirName() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // The path to the user's profile.
  const base::FilePath profile_path_;

  Delegate* const delegate_;
  MountObserver* const mount_observer_;
  network::NetworkConnectionTracker* const network_connection_tracker_;
  const base::Clock* const clock_;
  chromeos::disks::DiskMountManager* const disk_mount_manager_;
  std::unique_ptr<base::OneShotTimer> timer_;

  std::unique_ptr<DriveFsAuth> account_token_delegate_;

  // State specific to the current mount, or null if not mounted.
  std::unique_ptr<MountState> mount_state_;

  base::ObserverList<DriveFsHostObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsHost);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
