// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_GPM_PIN_CRYPTO_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_GPM_PIN_CRYPTO_H_

#include "chrome/browser/webauthn/gpm/gpm_pin.h"
#include "components/trusted_vault/gpm_pin_recovery_factor.h"

namespace webauthn {

enum class GpmPinVerificationResult {
  kMatch,
  kMismatch,
  kInvalidParameters,
};

// Hashes `pin` using scrypt with a fresh random 16-byte salt and infers
// `PinForm` (6 ASCII digits vs. arbitrary). `pin` must be non-empty. Must be
// called on a thread that allows long CPU work.
trusted_vault::GpmPinRecoveryFactor::HashedPin HashGpmPinSlowly(
    const GpmPin& pin);

// Verifies `pin` against `expected` using scrypt and a constant-time hash
// comparison. `pin` must be non-empty. Must be called on a thread that allows
// long CPU work. Returns `kInvalidParameters` if `expected` contains invalid
// KDF parameters (empty salt, non-32-byte hash, or unsupported scrypt
// difficulty).
GpmPinVerificationResult VerifyGpmPinSlowly(
    const GpmPin& pin,
    const trusted_vault::GpmPinRecoveryFactor::HashedPin& expected);

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_GPM_PIN_CRYPTO_H_
