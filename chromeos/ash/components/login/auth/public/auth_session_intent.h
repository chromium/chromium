// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_SESSION_INTENT_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_SESSION_INTENT_H_

#include "base/containers/enum_set.h"

namespace ash {

// This enum represents the intent of the authentication, i.e., the set of
// privileged operations that will be done after the authentication succeeds.
//
// The intent allows the underlying authentication implementation (e.g.,
// cryptohome) to choose eligible ways of authentication and also to use faster
// cryptographic schemes when viable.
enum class AuthSessionIntent {
  // Intent to decrypt the user's data protection keys. Authorizing for this
  // intent allows to access user data (e.g., for login), change the user's
  // authentication configuration (adding/removing/updating means of
  // authentication for the future).
  kDecrypt,
  // Intent to simply check whether the authentication succeeds. Authorizing for
  // this intent doesn't allow any privileged operation. It's suitable in
  // scenarios that only require a boolean result: can the user authenticate or
  // not
  kVerifyOnly,
  // Specific for the WebAuthN use case. Additionally to `kVerifyOnly`,
  // instructs Cryptohome that is should release the WebAuthN secret.
  kWebAuthn,
};

using AuthSessionIntents = base::EnumSet<AuthSessionIntent,
                                         AuthSessionIntent::kDecrypt,
                                         AuthSessionIntent::kWebAuthn>;
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_SESSION_INTENT_H_
