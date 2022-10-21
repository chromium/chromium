// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_host_verifier.h"

namespace ash {

namespace multidevice_setup {

FakeHostVerifier::FakeHostVerifier() = default;

FakeHostVerifier::~FakeHostVerifier() = default;

bool FakeHostVerifier::IsHostVerified() {
  return is_host_verified_;
}

void FakeHostVerifier::PerformAttemptVerificationNow() {
  ++num_verify_now_attempts_;
}

FakeHostVerifierObserver::FakeHostVerifierObserver() = default;

FakeHostVerifierObserver::~FakeHostVerifierObserver() = default;

void FakeHostVerifierObserver::OnHostVerified() {
  ++num_host_verifications_;
}

}  // namespace multidevice_setup

}  // namespace ash
