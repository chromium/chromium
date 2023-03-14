// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/integrity/misconfigured_user_cleaner.h"

#include "ash/public/cpp/session/session_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_directory_integrity_manager.h"

namespace ash {

MisconfiguredUserCleaner::MisconfiguredUserCleaner(
    PrefService* local_state,
    SessionController* session_controller)
    : local_state_(local_state),
      session_controller_(session_controller),
      mount_performer_(std::make_unique<MountPerformer>()) {}
MisconfiguredUserCleaner::~MisconfiguredUserCleaner() = default;

void MisconfiguredUserCleaner::CleanMisconfiguredUser() {
  user_manager::UserDirectoryIntegrityManager integrity_manager(local_state_);
  absl::optional<AccountId> misconfigured_user =
      integrity_manager.GetMisconfiguredUserAccountId();

  if (misconfigured_user.has_value()) {
    DoCleanup(integrity_manager, misconfigured_user.value());
  }
}

void MisconfiguredUserCleaner::ScheduleCleanup() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MisconfiguredUserCleaner::CleanMisconfiguredUser,
                     weak_factory_.GetWeakPtr()));
}

void MisconfiguredUserCleaner::DoCleanup(
    user_manager::UserDirectoryIntegrityManager& integrity_manager,
    const AccountId& account_id) {
  auto is_enterprise_managed = session_controller_->IsEnterpriseManaged();
  absl::optional<int> existing_users_count =
      session_controller_->GetExistingUsersCount();

  if (!existing_users_count.has_value()) {
    // We were not able to get the number of existing users, log error.
    LOG(ERROR) << "Unable to retrieve the number of existing users";
  } else if (!is_enterprise_managed && existing_users_count.value() == 0) {
    // user is owner, TPM ownership was established, powerwash the device.
    SessionManagerClient::Get()->StartDeviceWipe();
  } else {
    integrity_manager.RemoveUser(account_id);
    integrity_manager.ClearPrefs();
  }
}

}  // namespace ash
