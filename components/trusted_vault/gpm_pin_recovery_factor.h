// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_GPM_PIN_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_GPM_PIN_RECOVERY_FACTOR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "crypto/process_bound_string.h"

namespace trusted_vault {

// Interface for managing the Google Password Manager (GPM) PIN state in local
// Trusted Vault storage, tracking the local failed-attempt counter, syncing PIN
// state from the Security Domain Service (SDS), and uploading new hashed PINs
// to Vault and SDS for a single primary account.
//
// Interactive PIN hashing and verification live in `webauthn::GpmPinService`.
// Does not implement `LocalRecoveryFactor` yet because GPM PIN recovery is
// interactive via MagicArch and only registered in
// `SecurityDomainId::kPasskeys` today.
//
// All operations must be performed on the same sequence as
// `StandaloneTrustedVaultBackend`, and callbacks are never run synchronously.
class GpmPinRecoveryFactor {
 public:
  // Maximum consecutive failed local PIN verification attempts before the PIN
  // is locked locally.
  static constexpr int kMaxFailedAttempts = 5;

  // Format of the user's GPM PIN, used to select the PIN entry UI and validate
  // input.
  enum class PinForm {
    // Fixed 6-digit numeric PIN.
    kSixDigits,
    // Alphanumeric PIN of arbitrary length.
    kArbitrary,
  };

  // The scrypt-derived hash of a GPM PIN together with the KDF and format
  // parameters needed by `webauthn::GpmPinService` to recompute the hash when
  // verifying a user's PIN entry, or by `GpmPinRecoveryFactor` to wrap and
  // upload a newly set PIN. The raw PIN itself never enters
  // `//components/trusted_vault`.
  struct HashedPin {
    HashedPin(crypto::ProcessBoundString scrypt_hash,
              std::vector<uint8_t> hash_salt,
              int hash_difficulty,
              PinForm pin_form);
    HashedPin(const HashedPin&) = delete;
    HashedPin& operator=(const HashedPin&) = delete;
    HashedPin(HashedPin&&) noexcept;
    HashedPin& operator=(HashedPin&&) noexcept;
    ~HashedPin();

    crypto::ProcessBoundString scrypt_hash;
    std::vector<uint8_t> hash_salt;
    int hash_difficulty = 0;
    PinForm pin_form = PinForm::kSixDigits;
  };

  // Current GPM PIN state for the account: the unwrapped `HashedPin` synced
  // from SDS (or cached locally) together with the local consecutive
  // failed-attempt counter.
  struct PinState {
    explicit PinState(HashedPin hashed_pin, int failed_attempts = 0);
    PinState(const PinState&) = delete;
    PinState& operator=(const PinState&) = delete;
    PinState(PinState&&) noexcept;
    PinState& operator=(PinState&&) noexcept;
    ~PinState();

    HashedPin hashed_pin;
    int failed_attempts = 0;
  };

  // Result status of `FetchPinState()`.
  enum class FetchPinStatus {
    // A usable PIN was fetched from SDS (or loaded from local cache when
    // offline); `state` is populated.
    kPinSet,
    // The account has no GPM PIN registered.
    kNoPin,
    // PIN state could not be determined (e.g. SDS unreachable with no local
    // cache, or missing passkey domain keys).
    kError,
  };

  using FetchPinStateCallback =
      base::OnceCallback<void(FetchPinStatus status,
                              std::optional<PinState> state)>;
  using UploadPinCallback = base::OnceCallback<void(bool success)>;

  GpmPinRecoveryFactor();
  GpmPinRecoveryFactor(const GpmPinRecoveryFactor&) = delete;
  GpmPinRecoveryFactor& operator=(const GpmPinRecoveryFactor&) = delete;
  virtual ~GpmPinRecoveryFactor();

  // Fetches the latest GPM PIN member from SDS, unwraps it with the `kPasskeys`
  // domain secret, updates local storage (preserving `failed_attempts` if the
  // hashed PIN is unchanged), and returns the resulting `PinState`. Falls back
  // to cached local state if SDS is unreachable.
  virtual void FetchPinState(FetchPinStateCallback callback) = 0;

  // Uploads a double-wrapped Folsom `Vault` entry, registers the GPM PIN member
  // on SDS, and persists `hashed_pin` locally with `failed_attempts` reset to
  // 0.
  virtual void UploadPin(HashedPin hashed_pin, UploadPinCallback callback) = 0;

  // Increments the consecutive failed local verification counter (clamped to
  // `kMaxFailedAttempts`) and returns the updated count.
  virtual int RecordFailedAttempt() = 0;

  // Resets the consecutive failed local verification counter to 0.
  virtual void ResetFailedAttempts() = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_GPM_PIN_RECOVERY_FACTOR_H_
