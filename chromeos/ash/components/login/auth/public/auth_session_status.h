// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_SESSION_STATUS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_SESSION_STATUS_H_

#include "base/containers/enum_set.h"

namespace ash {

// This enum represents possible authentication level of auth session.
// Note that this is not specific to cryptohome AuthSessions, and can be used
// with other services providing some level of authentication (e.g.
// SmartUnlock).
enum class AuthSessionLevel {
  // Indication that session is still valid (but might be non-authenticated).
  kSessionIsValid,
  // Session is authenticated by some service other than cryptohome, no
  // cryptohome operations can be performed.
  kAuthenticatedOther,
  // Lightweight authentication in cryptohome. Authentication step is faster
  // when lightweight authentication is used, but not all operations might be
  // authorized by such authsession.
  kCryptohomeLightweight,
  // Strong authsentication in cryptohome. Authentication step is longer, but
  // all cryptohome operations are available with this type of authentication.
  kCryptohomeStrong
};

using AuthSessionStatus = base::EnumSet<AuthSessionLevel,
                                        AuthSessionLevel::kSessionIsValid,
                                        AuthSessionLevel::kCryptohomeStrong>;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_SESSION_STATUS_H_
