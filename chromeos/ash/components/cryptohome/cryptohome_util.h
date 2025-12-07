// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CRYPTOHOME_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "components/account_id/account_id.h"

namespace cryptohome {

// Returns the cryptohome account id for the given account id.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
const std::string GetCryptohomeId(const AccountId& account_id);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
