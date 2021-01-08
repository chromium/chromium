// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade.h"

namespace account_manager {

AccountManagerFacade::AccountAdditionResult::AccountAdditionResult() = default;
AccountManagerFacade::AccountAdditionResult::AccountAdditionResult(
    Status status,
    AccountKey account)
    : status(status), account(account) {}
AccountManagerFacade::AccountAdditionResult::AccountAdditionResult(
    Status status,
    GoogleServiceAuthError error)
    : status(status), error(error) {}
AccountManagerFacade::AccountAdditionResult::~AccountAdditionResult() = default;

AccountManagerFacade::AccountManagerFacade() = default;
AccountManagerFacade::~AccountManagerFacade() = default;

}  // namespace account_manager
