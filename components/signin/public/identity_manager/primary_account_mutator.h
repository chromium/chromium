// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_H_

#include <string>

#include "build/build_config.h"

namespace signin_metrics {
enum ProfileSignout : int;
enum class SignoutDelete;
}  // namespace signin_metrics

struct CoreAccountId;

namespace signin {

// PrimaryAccountMutator is the interface to set and clear the primary account
// (see IdentityManager for more information).
//
// This interface has concrete implementations on platform that
// support changing the signed-in state during the lifetime of the application.
// On other platforms, there is no implementation, and no instance will be
// available at runtime (thus accessors may return null).
class PrimaryAccountMutator {
 public:
  // Represents the options for handling the accounts known to the
  // IdentityManager upon calling ClearPrimaryAccount().
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.identitymanager
  enum class ClearAccountsAction {
    kDefault,    // Default action based on internal policy.
    kKeepAll,    // Keep all accounts.
    kRemoveAll,  // Remove all accounts.
  };

  PrimaryAccountMutator() = default;
  virtual ~PrimaryAccountMutator() = default;

  // PrimaryAccountMutator is non-copyable, non-moveable.
  PrimaryAccountMutator(PrimaryAccountMutator&& other) = delete;
  PrimaryAccountMutator const& operator=(PrimaryAccountMutator&& other) =
      delete;

  PrimaryAccountMutator(const PrimaryAccountMutator& other) = delete;
  PrimaryAccountMutator const& operator=(const PrimaryAccountMutator& other) =
      delete;

  // Marks the account with |account_id| as the primary account, and returns
  // whether the operation succeeded or not. To succeed, this requires that:
  //    - the account is known by the IdentityManager.
  // On non-ChromeOS platforms, this additionally requires that:
  //    - setting the primary account is allowed,
  //    - the account username is allowed by policy,
  //    - there is not already a primary account set.
  // TODO(https://crbug.com/983124): Investigate adding all the extra
  // requirements on ChromeOS as well.
  virtual bool SetPrimaryAccount(const CoreAccountId& account_id) = 0;

#if defined(OS_CHROMEOS)
  // Updates the info of the account corresponding to (|gaia_id|, |email|),
  // marks it as the primary account, and returns whether the operation
  // succeeded or not. Currently, this method is guaranteed to succeed.
  // NOTE: Unlike SetPrimaryAccount(), this method does not require that the
  // account is known by IdentityManager. The reason is that there are
  // contexts on ChromeOS where the primary account is not guaranteed to be
  // known by IdentityManager when it is set.
  // TODO(https://crbug.com/987955): Remove this API.
  virtual bool DeprecatedSetPrimaryAccountAndUpdateAccountInfo(
      const std::string& gaia_id,
      const std::string& email) = 0;
#endif

#if !defined(OS_CHROMEOS)
  // Clears the primary account, and returns whether the operation
  // succeeded or not. Depending on |action|, the other accounts
  // known to the IdentityManager may be deleted.
  virtual bool ClearPrimaryAccount(
      ClearAccountsAction action,
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) = 0;
#endif
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_H_
