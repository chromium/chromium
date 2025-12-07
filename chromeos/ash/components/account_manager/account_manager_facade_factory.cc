// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/account_manager/account_manager_facade_factory.h"

#include <string>

#include "chromeos/ash/components/account_manager/account_manager_factory.h"

namespace ash {
account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  return ash::AccountManagerFactory::Get()->GetAccountManagerFacade(
      profile_path);
}

}  // namespace ash
