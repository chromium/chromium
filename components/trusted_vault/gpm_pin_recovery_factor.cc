// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/gpm_pin_recovery_factor.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "crypto/process_bound_string.h"

namespace trusted_vault {

GpmPinRecoveryFactor::HashedPin::HashedPin(
    crypto::ProcessBoundString scrypt_hash,
    std::vector<uint8_t> hash_salt,
    int hash_difficulty,
    PinForm pin_form)
    : scrypt_hash(std::move(scrypt_hash)),
      hash_salt(std::move(hash_salt)),
      hash_difficulty(hash_difficulty),
      pin_form(pin_form) {}

GpmPinRecoveryFactor::HashedPin::HashedPin(HashedPin&&) noexcept = default;

GpmPinRecoveryFactor::HashedPin& GpmPinRecoveryFactor::HashedPin::operator=(
    HashedPin&&) noexcept = default;

GpmPinRecoveryFactor::HashedPin::~HashedPin() = default;

GpmPinRecoveryFactor::PinState::PinState(HashedPin hashed_pin,
                                         int failed_attempts)
    : hashed_pin(std::move(hashed_pin)), failed_attempts(failed_attempts) {}

GpmPinRecoveryFactor::PinState::PinState(PinState&&) noexcept = default;

GpmPinRecoveryFactor::PinState& GpmPinRecoveryFactor::PinState::operator=(
    PinState&&) noexcept = default;

GpmPinRecoveryFactor::PinState::~PinState() = default;

GpmPinRecoveryFactor::GpmPinRecoveryFactor() = default;

GpmPinRecoveryFactor::~GpmPinRecoveryFactor() = default;

}  // namespace trusted_vault
