// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/session/session_termination_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kLockedToSingleUserRebootDescription[] = "Reboot forced by policy";
const char kRemoteCommandSignoutRebootDescription[] =
    "Reboot remote command (sign out)";

SessionTerminationManager* g_instance = nullptr;

void Reboot(power_manager::RequestRestartReason reason,
            const std::string& description) {
  chromeos::PowerManagerClient::Get()->RequestRestart(reason, description);
}

}  // namespace

SessionTerminationManager::SessionTerminationManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

SessionTerminationManager::~SessionTerminationManager() {
  g_instance = nullptr;
}

// static
SessionTerminationManager* SessionTerminationManager::Get() {
  return g_instance;
}

void SessionTerminationManager::AddObserver(
    SessionTerminationManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void SessionTerminationManager::RemoveObserver(
    SessionTerminationManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SessionTerminationManager::StopSession(
    login_manager::SessionStopReason reason) {
  for (auto& observer : observers_) {
    observer.OnSessionWillBeTerminated();
  }

  if (is_locked_to_single_user_) {
    // If the device is locked to single user, it must reboot on sign out.
    Reboot(power_manager::REQUEST_RESTART_OTHER,
           kLockedToSingleUserRebootDescription);
  } else if (should_reboot_on_signout_) {
    if (before_reboot_callback_) {
      std::move(before_reboot_callback_).Run();
    }
    Reboot(power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT,
           kRemoteCommandSignoutRebootDescription);
  } else {
    SessionManagerClient::Get()->StopSession(reason);
  }
}

void SessionTerminationManager::RebootIfNecessary() {
  CryptohomeMiscClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&SessionTerminationManager::DidWaitForServiceToBeAvailable,
                     weak_factory_.GetWeakPtr()));
}

void SessionTerminationManager::SetDeviceLockedToSingleUser() {
  is_locked_to_single_user_ = true;
}

void SessionTerminationManager::SetDeviceRebootOnSignoutForRemoteCommand(
    base::OnceClosure before_reboot_callback) {
  should_reboot_on_signout_ = true;
  before_reboot_callback_ = std::move(before_reboot_callback);
}

bool SessionTerminationManager::IsLockedToSingleUser() {
  return is_locked_to_single_user_;
}

void SessionTerminationManager::DidWaitForServiceToBeAvailable(
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "WaitForServiceToBeAvailable failed.";
    return;
  }
  CryptohomeMiscClient::Get()->GetLoginStatus(
      user_data_auth::GetLoginStatusRequest(),
      base::BindOnce(&SessionTerminationManager::RebootIfNecessaryProcessReply,
                     weak_factory_.GetWeakPtr()));
}

void SessionTerminationManager::ProcessCryptohomeLoginStatusReply(
    const std::optional<user_data_auth::GetLoginStatusReply>& reply) {
  if (!reply.has_value() ||
      reply->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "Login status request failed, error: "
               << (reply.has_value() ? reply->error() : 0);
    return;
  }
  if (reply->is_locked_to_single_user()) {
    is_locked_to_single_user_ = true;
  }
}

void SessionTerminationManager::RebootIfNecessaryProcessReply(
    std::optional<user_data_auth::GetLoginStatusReply> reply) {
  ProcessCryptohomeLoginStatusReply(reply);
  if (is_locked_to_single_user_) {
    Reboot(power_manager::REQUEST_RESTART_OTHER,
           kLockedToSingleUserRebootDescription);
  }
}

}  // namespace ash
