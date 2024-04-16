// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_H_

#include <string>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

namespace signin_metrics {
enum class SourceForRefreshTokenOperation;
}

struct CoreAccountId;

namespace signin {

enum class Tribool;

// AccountsMutator is the interface to support seeding of account info and
// mutation of refresh tokens for the user's Gaia accounts.
class AccountsMutator {
 public:
  AccountsMutator() = default;

  AccountsMutator(const AccountsMutator&) = delete;
  AccountsMutator& operator=(const AccountsMutator&) = delete;

  virtual ~AccountsMutator() = default;

  // Updates the information of the account associated with |gaia_id|, first
  // adding that account to the system if it is not known.
  // Passing `signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN` preserves the
  // current access point if it's already set.
  virtual CoreAccountId AddOrUpdateAccount(
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
          ) = 0;

  // Updates the information about account identified by |account_id|.
  // If kUnknown is passed, the attribute is not updated.
  virtual void UpdateAccountInfo(const CoreAccountId& account_id,
                                 Tribool is_child_account,
                                 Tribool is_under_advanced_protection) = 0;

  // Removes the account given by |account_id|. Also revokes the token
  // server-side if needed.
  virtual void RemoveAccount(
      const CoreAccountId& account_id,
      signin_metrics::SourceForRefreshTokenOperation source) = 0;

  // Removes all accounts.
  virtual void RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation source) = 0;

  // Invalidates the refresh token of the primary account.
  // The primary account must necessarily be set by the time this method
  // is invoked.
  virtual void InvalidateRefreshTokenForPrimaryAccount(
      signin_metrics::SourceForRefreshTokenOperation source) = 0;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Removes the credentials associated to account_id from the internal storage,
  // and moves them to |target|. The credentials are not revoked on the server,
  // but the IdentityManager::Observer::OnRefreshTokenRemovedForAccount()
  // notification is sent to the observers. Also recreates a new device ID for
  // this mutator. The device ID of the current mutator is not moved to the
  // target mutator.
  virtual void MoveAccount(AccountsMutator* target,
                           const CoreAccountId& account_id) = 0;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Seeds account into AccountTrackerService. Used by UserSessionManager to
  // manually seed the primary account before credentials are loaded.
  // TODO(crbug.com/40176006): Remove after adding an account cache to
  // AccountManagerFacade.
  virtual CoreAccountId SeedAccountInfo(const std::string& gaia,
                                        const std::string& email) = 0;
#endif
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_H_
