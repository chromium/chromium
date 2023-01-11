// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/integrity/misconfigured_user_cleaner.h"

#include "ash/public/cpp/session/session_controller.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_directory_integrity_manager.h"

namespace ash {

MisconfiguredUserCleaner::MisconfiguredUserCleaner(PrefService* local_state)
    : local_state_(local_state),
      mount_performer_(std::make_unique<MountPerformer>()) {}
MisconfiguredUserCleaner::~MisconfiguredUserCleaner() = default;

void MisconfiguredUserCleaner::CleanMisconfiguredUser() {
  user_manager::UserDirectoryIntegrityManager integrity_manager(local_state_);
  absl::optional<AccountId> incomplete_user =
      integrity_manager.GetMisconfiguredUser();

  if (!incomplete_user.has_value()) {
    return;
  }

  auto* session_controller = SessionController::Get();
  auto is_enterprise_managed = session_controller->IsEnterpriseManaged();
  absl::optional<int> existing_users_count =
      session_controller->GetExistingUsersCount();
  if (!existing_users_count.has_value()) {
    // We were not able to get the number of existing users, log error.
    LOG(ERROR) << "Unable to retrieve the number of existing users";
  } else if (!is_enterprise_managed && existing_users_count.value() == 0) {
    // user is owner, TPM ownership was established, powerwash the device.
    SessionManagerClient::Get()->StartDeviceWipe();
  } else {
    // non-owner, simply remove the homedir.
    cryptohome::AccountIdentifier identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(
            incomplete_user.value());

    RemoveUserDirectory(identifier);
  }
}

void MisconfiguredUserCleaner::RemoveUserDirectory(
    const cryptohome::AccountIdentifier& user) {
  mount_performer_->RemoveUserDirectoryByIdentifier(
      user, base::BindOnce(&MisconfiguredUserCleaner::OnCleanMisconfiguredUser,
                           weak_factory_.GetWeakPtr()));
}

void MisconfiguredUserCleaner::OnCleanMisconfiguredUser(
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    // TODO(b/239420309): add retry logic.
    LOG(ERROR) << "Unable to clean misconfigured user's directory "
               << error->get_cryptohome_code();
  }

  // User cleanup successful, clear prefs.
  user_manager::UserDirectoryIntegrityManager integrity_manager(local_state_);
  integrity_manager.ClearKnownUserPrefs();
  integrity_manager.ClearPrefs();
}

}  // namespace ash
