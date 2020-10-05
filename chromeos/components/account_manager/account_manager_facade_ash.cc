// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/account_manager/account_manager_facade_ash.h"

#include "chromeos/components/account_manager/account_manager.h"

namespace chromeos {

AccountManagerFacadeAsh::AccountManagerFacadeAsh(
    AccountManager* account_manager)
    : account_manager_(account_manager) {}

AccountManagerFacadeAsh::~AccountManagerFacadeAsh() = default;

bool AccountManagerFacadeAsh::IsInitialized() {
  return account_manager_->IsInitialized();
}

}  // namespace chromeos
