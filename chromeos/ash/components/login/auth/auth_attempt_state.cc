// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_attempt_state.h"

#include <utility>

namespace ash {

AuthAttemptState::AuthAttemptState(std::unique_ptr<UserContext> user_context)
    : user_context(std::move(user_context)) {
  DCHECK(this->user_context);
}

AuthAttemptState::~AuthAttemptState() = default;

void AuthAttemptState::RecordOnlineLoginComplete() {
  online_complete_ = true;
}

void AuthAttemptState::RecordCryptohomeStatus(
    cryptohome::MountError cryptohome_code) {
  cryptohome_complete_ = true;
  cryptohome_code_ = cryptohome_code;
}

void AuthAttemptState::RecordUsernameHash(const std::string& username_hash) {
  user_context->SetUserIDHash(username_hash);
  username_hash_obtained_ = true;
  username_hash_valid_ = true;
}

void AuthAttemptState::RecordUsernameHashFailed() {
  username_hash_obtained_ = true;
  username_hash_valid_ = false;
}

void AuthAttemptState::UsernameHashRequested() {
  username_hash_obtained_ = false;
}

void AuthAttemptState::ResetCryptohomeStatus() {
  cryptohome_complete_ = false;
  cryptohome_code_ = cryptohome::MOUNT_ERROR_NONE;
}

bool AuthAttemptState::online_complete() {
  return online_complete_;
}

bool AuthAttemptState::cryptohome_complete() {
  return cryptohome_complete_;
}

cryptohome::MountError AuthAttemptState::cryptohome_code() {
  return cryptohome_code_;
}

bool AuthAttemptState::username_hash_obtained() {
  return username_hash_obtained_;
}

bool AuthAttemptState::username_hash_valid() {
  return username_hash_obtained_;
}

}  // namespace ash
