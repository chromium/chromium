// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/session/session_termination_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

SessionTerminationManager* g_instance = nullptr;

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

void SessionTerminationManager::StopSession() {
  // If the device is locked to single user, it must reboot on sign out.
  if (is_locked_to_single_user_) {
    Reboot();
  } else {
    SessionManagerClient::Get()->StopSession();
  }
}

void SessionTerminationManager::RebootIfNecessary() {
  CryptohomeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&SessionTerminationManager::DidWaitForServiceToBeAvailable,
                     weak_factory_.GetWeakPtr()));
}

void SessionTerminationManager::SetDeviceLockedToSingleUser() {
  is_locked_to_single_user_ = true;
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
  CryptohomeClient::Get()->GetTpmStatus(
      cryptohome::GetTpmStatusRequest(),
      base::BindOnce(&SessionTerminationManager::RebootIfNecessaryProcessReply,
                     weak_factory_.GetWeakPtr()));
}

void SessionTerminationManager::ProcessTpmStatusReply(
    const base::Optional<cryptohome::BaseReply>& reply) {
  if (!reply.has_value() || reply->has_error() ||
      !reply->HasExtension(cryptohome::GetTpmStatusReply::reply)) {
    LOG(ERROR) << "TPM status request failed, error: "
               << (reply.has_value() && reply->has_error() ? reply->error()
                                                           : 0);
    return;
  }
  auto reply_proto = reply->GetExtension(cryptohome::GetTpmStatusReply::reply);
  if (reply_proto.has_is_locked_to_single_user() &&
      reply_proto.is_locked_to_single_user()) {
    is_locked_to_single_user_ = true;
  }
}

void SessionTerminationManager::Reboot() {
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "Reboot forced by policy");
}

void SessionTerminationManager::RebootIfNecessaryProcessReply(
    base::Optional<cryptohome::BaseReply> reply) {
  ProcessTpmStatusReply(reply);
  if (is_locked_to_single_user_)
    Reboot();
}

}  // namespace chromeos
