// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_CRYPTOHOME_KEY_CONSTANTS_H_
#define CHROMEOS_LOGIN_AUTH_CRYPTOHOME_KEY_CONSTANTS_H_

#include "base/component_export.h"

namespace chromeos {

COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
extern const char kCryptohomeGaiaKeyLabel[];

COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
extern const char kCryptohomePinLabel[];

COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
extern const char kCryptohomeWildcardLabel[];

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source code migration is finished.
namespace ash {
using ::chromeos::kCryptohomeGaiaKeyLabel;
}

#endif  // CHROMEOS_LOGIN_AUTH_CRYPTOHOME_KEY_CONSTANTS_H_
