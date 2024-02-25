// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
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

  AccountsMutatorImpl(const AccountsMutatorImpl&) = delete;
  AccountsMutatorImpl& operator=(const AccountsMutatorImpl&) = delete;

  ~AccountsMutatorImpl() override;

  // AccountsMutator:
  CoreAccountId AddOrUpdateAccount(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& refresh_token,
      bool is_under_advanced_protection,
      signin_metrics::AccessPoint access_point,
      signin_metrics::SourceForRefreshTokenOperation source
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      const std::vector<uint8_t>& wrapped_binding_key = std::vector<uint8_t>()
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
          ) override;
  void UpdateAccountInfo(const CoreAccountId& account_id,
                         Tribool is_child_account,
                         Tribool is_under_advanced_protection) override;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  CoreAccountId SeedAccountInfo(const std::string& gaia,
                                const std::string& email) override;
#endif

 private:
  raw_ptr<ProfileOAuth2TokenService> token_service_;
  raw_ptr<AccountTrackerService> account_tracker_service_;
  raw_ptr<PrimaryAccountManager> primary_account_manager_;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  raw_ptr<PrefService> pref_service_;
#endif
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_IMPL_H_
