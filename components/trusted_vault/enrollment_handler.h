// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_ENROLLMENT_HANDLER_H_
#define COMPONENTS_TRUSTED_VAULT_ENROLLMENT_HANDLER_H_

#include "base/functional/callback_forward.h"

namespace trusted_vault {

// Interface for an enrollment handler.
// Classes that implement this interface are used by
// `StandaloneTrustedVaultBackend` to check eligibility and enroll a primary
// account into a security domain without user interaction.
// All operations on `EnrollmentHandler` must be performed on the same sequence
// as `StandaloneTrustedVaultBackend`.
class EnrollmentHandler {
 public:
  // Invoked with true if the account is eligible for non-interactive
  // enrollment into the security domain, or false otherwise.
  using CanEnrollCallback = base::OnceCallback<void(bool)>;

  EnrollmentHandler() = default;
  EnrollmentHandler(const EnrollmentHandler&) = delete;
  EnrollmentHandler& operator=(const EnrollmentHandler&) = delete;
  virtual ~EnrollmentHandler() = default;

  // Asynchronously checks whether the account can be enrolled into the
  // security domain without user interaction.
  virtual void CanEnroll(CanEnrollCallback callback) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_ENROLLMENT_HANDLER_H_
