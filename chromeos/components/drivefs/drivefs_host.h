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
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "services/identity/public/mojom/identity_manager.mojom.h"

namespace drive {
class DriveNotificationManager;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace disks {
class DiskMountManager;
}
}  // namespace chromeos

namespace drivefs {

class DriveFsHostObserver;

// A host for a DriveFS process. In addition to managing its lifetime via
// mounting and unmounting, it also bridges between the DriveFS process and the
// file manager.
class COMPONENT_EXPORT(DRIVEFS) DriveFsHost {
 public:
  // Public for overriding in tests. A default implementation is used under
  // normal conditions.
  class MojoConnectionDelegate {
   public:
    virtual ~MojoConnectionDelegate() = default;

    // Prepare the mojo connection to be used to communicate with the DriveFS
    // process. Returns the mojo handle to use for bootstrapping.
    virtual mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() = 0;

    // Accepts the mojo connection over |handle|.
    virtual void AcceptMojoConnection(base::ScopedFD handle) = 0;
  };

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

  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;
    virtual service_manager::Connector* GetConnector() = 0;
    virtual const AccountId& GetAccountId() = 0;
    virtual std::string GetObfuscatedAccountId() = 0;
    virtual drive::DriveNotificationManager& GetDriveNotificationManager() = 0;
    virtual std::unique_ptr<OAuth2MintTokenFlow> CreateMintTokenFlow(
        OAuth2MintTokenFlow::Delegate* delegate,
        const std::string& client_id,
        const std::string& app_id,
        const std::vector<std::string>& scopes);
    virtual std::unique_ptr<MojoConnectionDelegate>
    CreateMojoConnectionDelegate();

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  DriveFsHost(const base::FilePath& profile_path,
              Delegate* delegate,
              MountObserver* mount_observer,
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

 private:
  class AccountTokenDelegate;
  class MountState;

  SEQUENCE_CHECKER(sequence_checker_);

  // The path to the user's profile.
  const base::FilePath profile_path_;

  Delegate* const delegate_;
  MountObserver* const mount_observer_;
  const base::Clock* const clock_;
  chromeos::disks::DiskMountManager* const disk_mount_manager_;
  std::unique_ptr<base::OneShotTimer> timer_;

  std::unique_ptr<AccountTokenDelegate> account_token_delegate_;

  // State specific to the current mount, or null if not mounted.
  std::unique_ptr<MountState> mount_state_;

  base::ObserverList<DriveFsHostObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsHost);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_H_
