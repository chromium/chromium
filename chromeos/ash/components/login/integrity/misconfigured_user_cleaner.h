// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_INTEGRITY_MISCONFIGURED_USER_CLEANER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_INTEGRITY_MISCONFIGURED_USER_CLEANER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "components/user_manager/user_directory_integrity_manager.h"

class AccountId;
class PrefService;

namespace user_manager {
class UserDirectoryIntegrityManager;
}

namespace ash {

class SessionController;

// Cleans up the cryptohome of an incomplete user marked by
// `UserDirectoryIntegrityManager`.
// TODO(b/239420309): Implement retry logic in case of errors during cryptohome
// removal.
class COMPONENT_EXPORT(ASH_LOGIN_INTEGRITY) MisconfiguredUserCleaner {
 public:
  MisconfiguredUserCleaner(PrefService* local_state,
                           SessionController* session_controller);

  // Not copyable or movable.
  MisconfiguredUserCleaner(const MisconfiguredUserCleaner&) = delete;
  MisconfiguredUserCleaner& operator=(const MisconfiguredUserCleaner&) = delete;

  ~MisconfiguredUserCleaner();

  // Checks if any user has been incompletely created in the previous boot and
  // if so clean them up.
  void CleanMisconfiguredUser();

  // Must be called on the UI thread. Schedules misconfigured user cleanup if
  // any did not successfully go through the user creation process during the
  // previous boot. Misconfigured users will not be shown in the login UI, as we
  // filter them as part of `UserManagerImpl::EnsureUsersLoaded`.
  void ScheduleCleanup();

 private:
  void DoCleanup(user_manager::UserDirectoryIntegrityManager&,
                 const AccountId&,
                 user_manager::UserDirectoryIntegrityManager::CleanupStrategy);

  void OnStartDeviceWipe(bool result);

  const raw_ptr<PrefService, DanglingUntriaged> local_state_;

  // We expect `SessionController` to always outlive this class as it is owned
  // by `ash::Shell` and destroyed in
  // `ChromeBrowserMainPartsAsh::PostMainMessageLoopRun`, before
  // `ChromeBrowserMainPartsAsh`, the owner of this class.
  const raw_ptr<SessionController, DanglingUntriaged> session_controller_;

  std::unique_ptr<MountPerformer> mount_performer_;

  base::WeakPtrFactory<MisconfiguredUserCleaner> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_INTEGRITY_MISCONFIGURED_USER_CLEANER_H_
