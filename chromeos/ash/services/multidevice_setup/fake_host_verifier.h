// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_H_

#include "chromeos/ash/services/multidevice_setup/host_verifier.h"

namespace ash {

namespace multidevice_setup {

// Test HostVerifier implementation.
class FakeHostVerifier : public HostVerifier {
 public:
  FakeHostVerifier();

  FakeHostVerifier(const FakeHostVerifier&) = delete;
  FakeHostVerifier& operator=(const FakeHostVerifier&) = delete;

  ~FakeHostVerifier() override;

  void set_is_host_verified(bool is_host_verified) {
    is_host_verified_ = is_host_verified;
  }

  size_t num_verify_now_attempts() { return num_verify_now_attempts_; }

  using HostVerifier::NotifyHostVerified;

 private:
  // HostVerifier:
  bool IsHostVerified() override;
  void PerformAttemptVerificationNow() override;

  bool is_host_verified_ = false;
  size_t num_verify_now_attempts_ = 0u;
};

// Test HostVerifier::Observer implementation.
class FakeHostVerifierObserver : public HostVerifier::Observer {
 public:
  FakeHostVerifierObserver();

  FakeHostVerifierObserver(const FakeHostVerifierObserver&) = delete;
  FakeHostVerifierObserver& operator=(const FakeHostVerifierObserver&) = delete;

  ~FakeHostVerifierObserver() override;

  size_t num_host_verifications() { return num_host_verifications_; }

 private:
  // HostVerifier::Observer:
  void OnHostVerified() override;

  size_t num_host_verifications_ = 0u;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_H_
