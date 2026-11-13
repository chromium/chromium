// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_ENROLLMENT_HANDLER_H_
#define COMPONENTS_TRUSTED_VAULT_ENROLLMENT_HANDLER_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback_forward.h"

namespace trusted_vault {

enum class TrustedVaultCanEnrollOutcomeForUMA;
enum class TrustedVaultEnrollOutcomeForUMA;

// Interface for an enrollment handler.
// Classes that implement this interface are used by
// `StandaloneTrustedVaultBackend` to check eligibility and enroll a primary
// account into a security domain without user interaction.
// All operations on `EnrollmentHandler` must be performed on the same sequence
// as `StandaloneTrustedVaultBackend`.
class EnrollmentHandler {
 public:
  // Invoked with true if the account is eligible for non-interactive
  // enrollment into the security domain, or false otherwise, along with the
  // outcome for metric recording.
  using CanEnrollCallback =
      base::OnceCallback<void(bool, TrustedVaultCanEnrollOutcomeForUMA)>;
  // Invoked with the newly rotated shared key and its version on successful
  // enrollment into the security domain, or an empty vector and -1 on failure,
  // along with the outcome for metric recording.
  using EnrollCallback = base::OnceCallback<
      void(const std::vector<uint8_t>&, int, TrustedVaultEnrollOutcomeForUMA)>;

  EnrollmentHandler() = default;
  EnrollmentHandler(const EnrollmentHandler&) = delete;
  EnrollmentHandler& operator=(const EnrollmentHandler&) = delete;
  virtual ~EnrollmentHandler() = default;

  // Asynchronously checks whether the account can be enrolled into the
  // security domain without user interaction. Concurrent calls are supported.
  virtual void CanEnroll(CanEnrollCallback callback) = 0;

  // Initiates enrollment (factor registration and vault key rotation).
  // Invokes `callback` with the newly rotated vault key and key version on
  // success, or an empty vector and -1 on failure (including if another
  // `Enroll()` call is already in flight).
  virtual void Enroll(EnrollCallback callback) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_ENROLLMENT_HANDLER_H_
