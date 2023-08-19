// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CRYPTOHOME_KEY_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CRYPTOHOME_KEY_CONSTANTS_H_

#include "base/component_export.h"

namespace ash {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomeGaiaKeyLabel[];

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomeLocalPasswordKeyLabel[];

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomeGaiaKeyLegacyLabelPrefix[];

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomePinLabel[];

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomePublicMountLabel[];

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomeWildcardLabel[];

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
extern const char kCryptohomeRecoveryKeyLabel[];

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CRYPTOHOME_KEY_CONSTANTS_H_
