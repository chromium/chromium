// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_enrollment_manager.h"

namespace ash {

namespace device_sync {

FakeCryptAuthEnrollmentManager::FakeCryptAuthEnrollmentManager() = default;

FakeCryptAuthEnrollmentManager::~FakeCryptAuthEnrollmentManager() = default;

void FakeCryptAuthEnrollmentManager::Start() {
  has_started_ = true;
}

void FakeCryptAuthEnrollmentManager::FinishActiveEnrollment(
    bool success,
    base::Time enrollment_finish_time) {
  DCHECK(is_enrollment_in_progress_);
  is_enrollment_in_progress_ = false;

  if (success) {
    last_enrollment_time_ = enrollment_finish_time;
    is_enrollment_valid_ = true;
    is_recovering_from_failure_ = false;
  } else {
    is_recovering_from_failure_ = true;
  }

  NotifyEnrollmentFinished(success);
}

void FakeCryptAuthEnrollmentManager::ForceEnrollmentNow(
    cryptauth::InvocationReason invocation_reason,
    const std::optional<std::string>& session_id) {
  is_enrollment_in_progress_ = true;
  last_invocation_reason_ = invocation_reason;
  NotifyEnrollmentStarted();
}

bool FakeCryptAuthEnrollmentManager::IsEnrollmentValid() const {
  return is_enrollment_valid_;
}

base::Time FakeCryptAuthEnrollmentManager::GetLastEnrollmentTime() const {
  return last_enrollment_time_;
}

base::TimeDelta FakeCryptAuthEnrollmentManager::GetTimeToNextAttempt() const {
  return time_to_next_attempt_;
}

bool FakeCryptAuthEnrollmentManager::IsEnrollmentInProgress() const {
  return is_enrollment_in_progress_;
}

bool FakeCryptAuthEnrollmentManager::IsRecoveringFromFailure() const {
  return is_recovering_from_failure_;
}

std::string FakeCryptAuthEnrollmentManager::GetUserPublicKey() const {
  return user_public_key_;
}

std::string FakeCryptAuthEnrollmentManager::GetUserPrivateKey() const {
  return user_private_key_;
}

}  // namespace device_sync

}  // namespace ash
