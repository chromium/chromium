// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACADE_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACADE_FACTORY_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"

namespace account_manager {
class AccountManagerFacade;
}  // namespace account_manager

namespace ash {

// A factory function for getting platform specific implementations of
// |AccountManagerFacade|.
// Returns the |AccountManagerFacade| for the given |profile_path|.
// Note that |AccountManagerFacade| is independent of a |Profile|, and this is
// needed only because of Multi-Login on Chrome OS, and will be removed soon.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER)
account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACADE_FACTORY_H_
