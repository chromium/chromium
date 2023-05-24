// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_auth.h"
#include "chromeos/ash/components/drivefs/drivefs_session.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::disks {
class DiskMountManager;
}  // namespace ash::disks

namespace drive {
class DriveNotificationManager;
}  // namespace drive

namespace network {
class NetworkConnectionTracker;
}  // namespace network

namespace drivefs {

class DriveFsBootstrapListener;
class DriveFsHostObserver;

// A host for a DriveFS process. In addition to managing its lifetime via
// mounting and unmounting, it also bridges between the DriveFS process and the
// file manager.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsHost {
 public:
  using MountObserver = DriveFsSession::MountObserver;
  using DialogHandler = base::RepeatingCallback<void(
      const mojom::DialogReason&,
      base::OnceCallback<void(mojom::DialogResult)>)>;

  class Delegate : public DriveFsAuth::Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    ~Delegate() override = default;

    virtual drive::DriveNotificationManager& GetDriveNotificationManager() = 0;
    virtual std::unique_ptr<DriveFsBootstrapListener> CreateMojoListener();
    virtual base::FilePath GetMyFilesPath() = 0;
    virtual std::string GetLostAndFoundDirectoryName() = 0;
    virtual bool IsVerboseLoggingEnabled() = 0;
    virtual void ConnectToExtension(
        mojom::ExtensionConnectionParamsPtr params,
        mojo::PendingReceiver<mojom::NativeMessagingPort> port,
        mojo::PendingRemote<mojom::NativeMessagingHost> host,
        mojom::DriveFsDelegate::ConnectToExtensionCallback callback) = 0;
    virtual const std::string GetMachineRootID() = 0;
    virtual void PersistMachineRootID(const std::string& id) = 0;
  };

  DriveFsHost(const base::FilePath& profile_path,
              Delegate* delegate,
              MountObserver* mount_observer,
              network::NetworkConnectionTracker* network_connection_tracker,
              const base::Clock* clock,
              ash::disks::DiskMountManager* disk_mount_manager,
              std::unique_ptr<base::OneShotTimer> timer);

  DriveFsHost(const DriveFsHost&) = delete;
  DriveFsHost& operator=(const DriveFsHost&) = delete;

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

  SyncState GetSyncStateForPath(const base::FilePath& drive_path) const;

  // Starts DriveFs search query and returns whether it will be
  // performed localy or remotely. Assumes DriveFS to be mounted.
  mojom::QueryParameters::QuerySource PerformSearch(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback);

  void set_dialog_handler(DialogHandler dialog_handler) {
    dialog_handler_ = dialog_handler;
  }

 private:
  class AccountTokenDelegate;
  class MountState;

  std::string GetDefaultMountDirName() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // The path to the user's profile.
  const base::FilePath profile_path_;

  const raw_ptr<Delegate, ExperimentalAsh> delegate_;
  const raw_ptr<MountObserver, ExperimentalAsh> mount_observer_;
  const raw_ptr<network::NetworkConnectionTracker, ExperimentalAsh>
      network_connection_tracker_;
  const raw_ptr<const base::Clock, ExperimentalAsh> clock_;
  const raw_ptr<ash::disks::DiskMountManager, ExperimentalAsh>
      disk_mount_manager_;
  std::unique_ptr<base::OneShotTimer> timer_;

  std::unique_ptr<DriveFsAuth> account_token_delegate_;

  // State specific to the current mount, or null if not mounted.
  std::unique_ptr<MountState> mount_state_;

  base::ObserverList<DriveFsHostObserver>::Unchecked observers_;
  DialogHandler dialog_handler_;
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
