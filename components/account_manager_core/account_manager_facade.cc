// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade.h"

namespace account_manager {

// static
const char AccountManagerFacade::kAccountAdditionSource[] =
    "AccountManager.AccountAdditionSource";

AccountManagerFacade::Observer::Observer() = default;
AccountManagerFacade::Observer::~Observer() = default;

AccountManagerFacade::AccountManagerFacade() = default;
AccountManagerFacade::~AccountManagerFacade() = default;

}  // namespace account_manager
