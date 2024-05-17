// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "components/account_id/account_id.h"

namespace ash {

class UserContext;

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
//
// Important! These values are persisted in LocalState, do not renumber them.
enum class AuthPurpose {
  kLogin = 0,             // Authentication to sign in.
  kAuthSettings = 1,      // Access to the section of os://settings.
  kScreenUnlock = 2,      // Removing the lock screen.
  kWebAuthN = 3,          // Local user verification in WebAuthN flow,
                          // where ChromeOS device serves as FIDO2
                          // Authenticator.
  kUserVerification = 4,  // Local user verification e.g. in Chrome password
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
//
// Important! These values are persisted in LocalState, do not renumber them.
enum class AshAuthFactor {
  kGaiaPassword = 0,
  kCryptohomePin = 1,
  kSmartCard = 2,
  kSmartUnlock = 3,
  kRecovery = 4,
  kLegacyPin = 5,
  kLegacyFingerprint = 6,
  kLocalPassword = 7,
  kFingerprint = 8,
  kMaxValue = kFingerprint,
};

using AuthFactorsSet = base::EnumSet<AshAuthFactor,
                                     AshAuthFactor::kGaiaPassword,
                                     AshAuthFactor::kMaxValue>;

enum class AuthHubMode {
  kNone,         // State before initialization
  kLoginScreen,  // Login screen, no profile data available.
  kInSession     // In-session mode (including lock screen), user is fixed,
                 // but purposes might change,
};

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthAttemptVector {
  AccountId account;
  AuthPurpose purpose;

  bool operator==(const AuthAttemptVector&) const = default;
};

using BorrowContextCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>)>;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_
