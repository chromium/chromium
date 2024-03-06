// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/integrity/misconfigured_user_cleaner.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/command_line.h"
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
#include "chromeos/dbus/power/power_manager_client.h"
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
  std::optional<AccountId> misconfigured_user =
      integrity_manager.GetMisconfiguredUserAccountId();

  if (misconfigured_user.has_value()) {
    LOG(ERROR) << "Found a user without credentials set up at creation.";
    auto strategy = integrity_manager.GetMisconfiguredUserCleanupStrategy();
    DoCleanup(integrity_manager, misconfigured_user.value(), strategy);
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
    const AccountId& account_id,
    user_manager::UserDirectoryIntegrityManager::CleanupStrategy strategy) {
  bool ignore_owner_in_tests =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kCryptohomeIgnoreCleanupOwnershipForTesting);
  if (strategy == user_manager::UserDirectoryIntegrityManager::CleanupStrategy::
                      kSilentPowerwash) {
    if (!ignore_owner_in_tests) {
      LOG(WARNING) << "User is owner, removing the user requires powerwash.";
      // user is owner, TPM ownership was established, powerwash the device.
      SessionManagerClient::Get()->StartDeviceWipe(
          base::BindOnce(&MisconfiguredUserCleaner::OnStartDeviceWipe,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    LOG(WARNING) << "Treating owner user as non-owner due to test-only switch";
  }

  LOG(WARNING) << "User is non-owner, trigger user removal.";
  integrity_manager.RemoveUser(account_id);
  integrity_manager.ClearPrefs();
}

void MisconfiguredUserCleaner::OnStartDeviceWipe(bool result) {
  if (!result) {
    // If powerwash was not triggered, this could be due to session manager
    // either not getting the request, or session manager ignoring the request
    // because we are already in session.
    // In both cases, a restart would re-trigger misconfigured user cleanup as
    // `incomplete_login_user_account` pref would still be present in local
    // state.
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::RequestRestartReason::REQUEST_RESTART_OTHER,
        "Restarting for logged-in misconfigured user owner cleanup");
  }
}

}  // namespace ash
