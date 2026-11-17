// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_PIN_SERVICE_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_PIN_SERVICE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/webauthn/gpm_pin_availability.h"
#include "components/keyed_service/core/keyed_service.h"
#include "crypto/process_bound_string.h"

struct CoreAccountInfo;

namespace webauthn {

using GpmPin = base::StrongAlias<class GpmPinTag, crypto::ProcessBoundString>;

// Verifies and sets the Google Password Manager (GPM) PIN for the in-browser
// GPM passkey authenticator.
//
// Callbacks are never run synchronously.
//
// TODO(crbug.com/570170982): Back this with Trusted Vault. Until then every
// operation fails.
class GpmPinService : public KeyedService {
 public:
  // TODO(crbug.com/570170982): Replace or align with GPM PIN metadata in
  // //components/trusted_vault once that lands.
  struct PinState {
    GpmPinAvailability availability = GpmPinAvailability::kGpmPinUnset;
    // True if the PIN is alphanumeric rather than 6 digits. Only meaningful if
    // `availability` is `kGpmPinSetAndUsable`.
    bool is_arbitrary = false;
  };

  enum class VerifyPinResult {
    kSuccess,
    kIncorrectPin,
    kLocked,
    kError,
  };

  // Runs with `std::nullopt` if the state could not be determined.
  using PinStateCallback = base::OnceCallback<void(std::optional<PinState>)>;
  using VerifyPinCallback =
      base::OnceCallback<void(VerifyPinResult, int remaining_attempts)>;
  using SetPinCallback = base::OnceCallback<void(bool success)>;

  GpmPinService();
  GpmPinService(const GpmPinService&) = delete;
  GpmPinService& operator=(const GpmPinService&) = delete;
  ~GpmPinService() override;

  // Fetches the PIN from the security domain service, falling back to the
  // locally cached state if that fails. Call before prompting for the PIN so
  // that PIN changes on other devices are picked up.
  virtual void SyncPinState(const CoreAccountInfo& account,
                            PinStateCallback callback);

  virtual void VerifyPin(const CoreAccountInfo& account,
                         GpmPin pin,
                         VerifyPinCallback callback);

  // Replaces any existing PIN and resets the remaining attempts. Whether the
  // PIN is 6 digits or arbitrary is inferred from `new_pin`.
  virtual void SetPin(const CoreAccountInfo& account,
                      GpmPin new_pin,
                      SetPinCallback callback);
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_PIN_SERVICE_H_
