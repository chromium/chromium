// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {

// Helper class to determine if a given account is a managed (aka enterprise)
// account.
class AccountManagedStatusFinder : public signin::IdentityManager::Observer {
 public:
  // Whether an email belongs to an enterprise domain.
  enum class EmailEnterpriseStatus {
    // It's unknown whether an email belongs to an enterprise domain.
    kUnknown,
    // Email belongs to a well-known non-enterprise domain.
    kKnownNonEnterprise,
  };

  // Check whether the given account is known to be non-enterprise. Domains such
  // as gmail.com and googlemail.com are known to not be managed.
  static EmailEnterpriseStatus IsEnterpriseUserBasedOnEmail(
      const std::string& email);

  // Allows to register a domain that is recognized as non-enterprise for tests.
  // Note that `domain` needs to live until this method is invoked with nullptr.
  static void SetNonEnterpriseDomainForTesting(const char* domain);

  // The outcome of the managed-ness check.
  enum class Outcome {
    // Check isn't complete yet.
    kPending,
    // An error happened, e.g. the account was removed from IdentityManager.
    kError,
    // The account is a consumer (non-enterprise) account.
    kNonEnterprise,
    // The account is an enterprise account but *not* an @google.com one.
    kEnterprise,
    // The account is an @google.com enterprise account.
    kEnterpriseGoogleDotCom
  };

  // After an AccountManagedStatusFinder is instantiated, the account type may
  // or may not be known immediately. The `async_callback` will only be run if
  // the account type was *not* known immediately, i.e. if `GetOutcome()` was
  // still `kPending` when the constructor returned.
  AccountManagedStatusFinder(signin::IdentityManager* identity_manager,
                             const CoreAccountInfo& account,
                             base::OnceClosure async_callback);
  ~AccountManagedStatusFinder() override;

  const CoreAccountInfo& GetAccountInfo() const { return account_; }

  Outcome GetOutcome() const { return outcome_; }

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  void OutcomeDetermined(Outcome type);

  raw_ptr<signin::IdentityManager> identity_manager_;
  const CoreAccountInfo account_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::OnceClosure callback_;

  Outcome outcome_ = Outcome::kPending;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_
