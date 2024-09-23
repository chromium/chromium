// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/public/string_utils.h"

#include <ostream>

#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

std::ostream& operator<<(std::ostream& out, AuthPurpose purpose) {
  switch (purpose) {
#define PRINT(s)          \
  case AuthPurpose::k##s: \
    return out << #s;
    PRINT(Login)
    PRINT(AuthSettings)
    PRINT(ScreenUnlock)
    PRINT(WebAuthN)
    PRINT(UserVerification)
#undef PRINT
  }
}

std::ostream& operator<<(std::ostream& out, AshAuthFactor factor) {
  switch (factor) {
#define PRINT(s)            \
  case AshAuthFactor::k##s: \
    return out << #s;
    PRINT(GaiaPassword)
    PRINT(CryptohomePin)
    PRINT(SmartCard)
    PRINT(SmartUnlock)
    PRINT(Recovery)
    PRINT(LegacyPin)
    PRINT(LegacyFingerprint)
    PRINT(LocalPassword)
    PRINT(Fingerprint)
#undef PRINT
  }
}

std::ostream& operator<<(std::ostream& out, AuthHubMode mode) {
  switch (mode) {
#define PRINT(s)          \
  case AuthHubMode::k##s: \
    return out << #s;
    PRINT(None)
    PRINT(LoginScreen)
    PRINT(InSession)
#undef PRINT
  }
}

}  // namespace ash
