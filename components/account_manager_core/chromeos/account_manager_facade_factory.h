// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_FACADE_FACTORY_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_FACADE_FACTORY_H_

#include <string>

#include "base/component_export.h"
#include "build/chromeos_buildflags.h"

namespace account_manager {
class AccountManagerFacade;
class AccountManager;
}  // namespace account_manager

// A factory function for getting platform specific implementations of
// |AccountManagerFacade|.
// Returns the |AccountManagerFacade| for the given |profile_path|.
// Note that |AccountManagerFacade| is independent of a |Profile|, and this is
// needed only because of Multi-Login on Chrome OS, and will be removed soon.
account_manager::AccountManagerFacade* COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
    GetAccountManagerFacade(const std::string& profile_path);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Create a new instance of `account_manager::AccountManager` for tests. Should
// be called before the first call to `GetAccountManagerFacade()`. After this
// call `GetAccountManagerFacade()` will be returning an instance that is
// connected to `AccountManagerMojoService`.
void COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) CreateAshAccountManagerForTests();

// Return an `AccountManager` instance if it was created for tests,
// otherwise return `nullptr`.
account_manager::AccountManager* COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
    MaybeGetAshAccountManagerForTests();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_FACADE_FACTORY_H_
