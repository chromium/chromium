// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace signin_metrics {
enum class SourceForRefreshTokenOperation;
}

struct CoreAccountId;

namespace signin {

// AccountsMutator is the interface to support seeding of account info and
// mutation of refresh tokens for the user's Gaia accounts.
class AccountsMutator {
 public:
  AccountsMutator() = default;
  virtual ~AccountsMutator() = default;

  // Updates the information of the account associated with |gaia_id|, first
  // adding that account to the system if it is not known.
  virtual CoreAccountId AddOrUpdateAccount(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& refresh_token,
      bool is_under_advanced_protection,
      signin_metrics::SourceForRefreshTokenOperation source) = 0;

  // Updates the information about account identified by |account_id|.
  virtual void UpdateAccountInfo(
      const CoreAccountId& account_id,
      base::Optional<bool> is_child_account,
      base::Optional<bool> is_under_advanced_protection) = 0;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsMutator);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_MUTATOR_H_
