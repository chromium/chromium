// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_GPM_PASSKEYS_UTIL_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_GPM_PASSKEYS_UTIL_H_

#include "base/functional/callback_forward.h"

class Profile;

namespace webauthn {

// Returns true if the local browser-based GPM passkey authenticator feature
// flags (`device::kWebAuthnLocalGpmPasskeys` and
// `trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption`) are both
// enabled.
bool IsLocalGpmPasskeysEnabled();

// Determines asynchronously whether `profile` should use the local
// browser-based GPM passkey authenticator rather than the cloud enclave
// authenticator:
// - If `!IsLocalGpmPasskeysEnabled()`, runs `callback` with `false`.
// - If `kWebAuthnLocalGpmPasskeysMode` is `kAllUsers`, runs `callback` with
//   `true`.
// - If `kNewUsersOnly`, loads `EnclaveManager` if needed and runs `callback`
//   with `!is_registered` (or `true` if `EnclaveManager` is null for
//   `profile`).
void ShouldUseLocalGpmPasskeys(Profile* profile,
                               base::OnceCallback<void(bool)> callback);

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_GPM_PASSKEYS_UTIL_H_
