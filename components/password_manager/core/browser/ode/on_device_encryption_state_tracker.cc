// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"

#include <utility>

namespace password_manager {

OnDeviceEncryptionStateTracker::OnDeviceEncryptionStateTracker() = default;

OnDeviceEncryptionStateTracker::~OnDeviceEncryptionStateTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observer_list_) {
    observer.OnDeviceEncryptionStateTrackerShuttingDown(this);
  }
}

void OnDeviceEncryptionStateTracker::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void OnDeviceEncryptionStateTracker::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

OnDeviceEncryptionState OnDeviceEncryptionStateTracker::GetEncryptionState()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void OnDeviceEncryptionStateTracker::SetState(
    OnDeviceEncryptionState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == new_state) {
    return;
  }
  OnDeviceEncryptionState previous_state = std::exchange(state_, new_state);
  for (Observer& observer : observer_list_) {
    observer.OnDeviceEncryptionStateChanged(this, previous_state, state_);
  }
}

void OnDeviceEncryptionStateTracker::SetStateForTesting(
    OnDeviceEncryptionState new_state) {
  SetState(new_state);
}

}  // namespace password_manager
