// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_FACADE_FACTORY_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_FACADE_FACTORY_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"

namespace account_manager {
class AccountManagerFacade;
}  // namespace account_manager

// A factory function for getting platform specific implementations of
// |AccountManagerFacade|.
// Returns the |AccountManagerFacade| for the given |profile_path|.
// Note that |AccountManagerFacade| is independent of a |Profile|, and this is
// needed only because of Multi-Login on Chrome OS, and will be removed soon.
account_manager::AccountManagerFacade* COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
    GetAccountManagerFacade(const std::string& profile_path);

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_FACADE_FACTORY_H_
