// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_verifier.h"

#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace multidevice_setup {

HostVerifier::HostVerifier() = default;

HostVerifier::~HostVerifier() = default;

void HostVerifier::AttemptVerificationNow() {
  if (IsHostVerified()) {
    PA_LOG(ERROR) << "HostVerifier::AttemptVerificationNow(): Attempted to "
                  << "start verification, but the current host has already "
                  << "been verified.";
    NOTREACHED_IN_MIGRATION();
  }

  PerformAttemptVerificationNow();
}

void HostVerifier::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostVerifier::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostVerifier::NotifyHostVerified() {
  for (auto& observer : observer_list_)
    observer.OnHostVerified();
}

}  // namespace multidevice_setup

}  // namespace ash
