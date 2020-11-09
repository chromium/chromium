// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_UTIL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_UTIL_H_

#include "base/optional.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"

namespace account_manager {

// Returns `base::nullopt` if `mojom_account` cannot be parsed.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
base::Optional<account_manager::Account> FromMojoAccount(
    const crosapi::mojom::AccountPtr& mojom_account);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::AccountPtr ToMojoAccount(
    const account_manager::Account& account);

// Returns `base::nullopt` if `account_type` cannot be parsed.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
base::Optional<account_manager::AccountType> FromMojoAccountType(
    const crosapi::mojom::AccountType& account_type);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::AccountType ToMojoAccountType(
    const account_manager::AccountType& account_type);

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_UTIL_H_
