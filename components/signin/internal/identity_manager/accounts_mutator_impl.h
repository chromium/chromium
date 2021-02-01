// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_IMPL_H_

#include <string>

#include "base/macros.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"

namespace signin_metrics {
enum class SourceForRefreshTokenOperation;
}

class AccountTrackerService;
struct CoreAccountId;
class PrefService;
class PrimaryAccountManager;
class ProfileOAuth2TokenService;

namespace signin {

// Concrete implementation of the AccountsMutatorImpl interface.
class AccountsMutatorImpl : public AccountsMutator {
 public:
  explicit AccountsMutatorImpl(ProfileOAuth2TokenService* token_service,
                               AccountTrackerService* account_tracker_service,
                               PrimaryAccountManager* primary_account_manager,
                               PrefService* pref_service);
  ~AccountsMutatorImpl() override;

  // AccountsMutator:
  CoreAccountId AddOrUpdateAccount(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& refresh_token,
      bool is_under_advanced_protection,
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void UpdateAccountInfo(
      const CoreAccountId& account_id,
      base::Optional<bool> is_child_account,
      base::Optional<bool> is_under_advanced_protection) override;
  void RemoveAccount(
      const CoreAccountId& account_id,
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void InvalidateRefreshTokenForPrimaryAccount(
      signin_metrics::SourceForRefreshTokenOperation source) override;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void MoveAccount(AccountsMutator* target,
                   const CoreAccountId& account_id) override;
#endif

 private:
  ProfileOAuth2TokenService* token_service_;
  AccountTrackerService* account_tracker_service_;
  PrimaryAccountManager* primary_account_manager_;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  PrefService* pref_service_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorImpl);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_IMPL_H_
