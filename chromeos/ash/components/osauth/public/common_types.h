// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_

#include <string>

#include "base/containers/enum_set.h"
#include "base/unguessable_token.h"

namespace ash {

// This token represents authentication proof. It can be safely passed
// between components, and can be used to obtain authenticated
// 'UserContext' from `AuthSessionStorage` to perform authenticated
// operations.
// TODO(b/259528315): Once switch from QuickUnlockStorage is completed,
// replace it with StrongAlias or UnguessableToken.
using AuthProofToken = std::string;

// Authentication can be required for different scenarios,
// with some specifics or trade-offs. This enumeration allows
// to distinguish such scenarios when requesting authentication.
enum class AuthPurpose {
  kLogin,             // Authentication to sign in.
  kAuthSettings,      // Access to the section of os://settings.
  kScreenUnlock,      // Removing the lock screen.
  kWebAuthN,          // Local user verification in WebAuthN flow,
                      // where ChromeOS device serves as FIDO2 Authenticator.
  kUserVerification,  // Local user verification e.g. in Chrome password
                      // manager.
};

// Authentication factors (and their implementations) that can be used
// for interactive authentication in Ash.
// Not to be confused with cryptohome AuthFactors:
//   * Some factors (like SmartLock) are implemented without cryptohome;
//   * Some cryptohome factors (like Kiosk) are not used for regular user
//     authentication;
//   * Multiple factors (e.g. GAIA password and Local password) might be
//     backed by the same cryptohome factor.
enum class AshAuthFactor {
  kGaiaPassword,
  kCryptohomePin,
  kSmartCard,
  kSmartUnlock,
  kRecovery,
  kLegacyPin,
  kLegacyFingerprint,
};

using AuthFactorsSet = base::EnumSet<AshAuthFactor,
                                     AshAuthFactor::kGaiaPassword,
                                     AshAuthFactor::kLegacyFingerprint>;

enum AuthHubMode {
  kNone,         // State before initialization
  kLoginScreen,  // Login screen, no profile data available.
  kInSession     // In-session mode, user is fixed, but purposes might change.
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_
