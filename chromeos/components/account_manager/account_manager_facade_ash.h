// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACADE_ASH_H_
#define CHROMEOS_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACADE_ASH_H_

#include "components/account_manager_core/account_manager_facade.h"

namespace chromeos {

class AccountManager;

// Ash-chrome specific implementation of |AccountManagerFacade| that talks to
// |chromeos::AccountManager| in-process.
class COMPONENT_EXPORT(ACCOUNT_MANAGER) AccountManagerFacadeAsh
    : public ::account_manager::AccountManagerFacade {
 public:
  explicit AccountManagerFacadeAsh(AccountManager* account_manager);
  AccountManagerFacadeAsh(const AccountManagerFacadeAsh&) = delete;
  AccountManagerFacadeAsh& operator=(const AccountManagerFacadeAsh&) = delete;
  ~AccountManagerFacadeAsh() override;

  // AccountManagerFacade overrides:
  bool IsInitialized() override;
  void ShowAddAccountDialog(
      const AccountAdditionSource& source,
      base::OnceCallback<void(const AccountAdditionResult& result)> callback)
      override;
  void ShowReauthAccountDialog(const AccountAdditionSource& source,
                               const std::string& email) override;

 private:
  AccountManager* const account_manager_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACADE_ASH_H_
