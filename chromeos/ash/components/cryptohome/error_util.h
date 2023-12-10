// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_UTIL_H_

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/error_types.h"

namespace cryptohome {

// Checks if `error` actually contain error code.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
bool HasError(ErrorWrapper error);

// Checks if `value` matches given error codes, encapsulates
// all implementation details of error matching.
// TODO(b/246499081): add extra parameter based on user_data_auth::PrimaryAction
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
bool ErrorMatches(ErrorWrapper value,
                  ::user_data_auth::CryptohomeErrorCode error_code);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_UTIL_H_
