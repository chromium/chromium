// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_COMMON_TYPES_H_
#define COMPONENTS_USER_MANAGER_COMMON_TYPES_H_

#include <string>

#include "base/types/strong_alias.h"

namespace user_manager {

// Cryptohome account identifier.
// We use a strong alias to avoid accidentally mixing it up with other variables
// of type `std::string`.
using CryptohomeId = base::StrongAlias<class CryptohomeIdTag, std::string>;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_COMMON_TYPES_H_
