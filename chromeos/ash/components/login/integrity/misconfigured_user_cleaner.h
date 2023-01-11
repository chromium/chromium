// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_INTEGRITY_MISCONFIGURED_USER_CLEANER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_INTEGRITY_MISCONFIGURED_USER_CLEANER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace ash {

// Cleans up the cryptohome of an incomplete user marked by
// `UserDirectoryIntegrityManager`.
class COMPONENT_EXPORT(ASH_LOGIN_INTEGRITY) MisconfiguredUserCleaner {
 public:
  explicit MisconfiguredUserCleaner(PrefService* local_state);

  // Not copyable or movable.
  MisconfiguredUserCleaner(const MisconfiguredUserCleaner&) = delete;
  MisconfiguredUserCleaner& operator=(const MisconfiguredUserCleaner&) = delete;

  ~MisconfiguredUserCleaner();

  // Checks if any user has been incompletely created in the previous boot and
  // if so clean them up.
  void CleanMisconfiguredUser();

 private:
  // Callback to `MountPerformer::RemoveUsrDirectoryByIdentifier`.
  void OnCleanMisconfiguredUser(absl::optional<AuthenticationError> error);

  // Calls MountPerformer to remove the unusable user's home directory
  void RemoveUserDirectory(const cryptohome::AccountIdentifier& user);

  PrefService* const local_state_;
  std::unique_ptr<MountPerformer> mount_performer_;
  base::WeakPtrFactory<MisconfiguredUserCleaner> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_INTEGRITY_MISCONFIGURED_USER_CLEANER_H_
