// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {

// Helper class to determine if a given account is a managed (aka enterprise)
// account.
class AccountManagedStatusFinder : public signin::IdentityManager::Observer {
 public:
  using Outcome = signin::AccountManagedStatusFinderOutcome;

  // Returns whether the given domain *may* be an enterprise (aka managed)
  // domain, i.e. definitely not a consumer domain. Domains such as gmail.com
  // and hotmail.com (and many others) are known to not be managed, even without
  // the more sophisticated checks implemented by this class.
  static bool MayBeEnterpriseDomain(const std::string& email_domain);

  // Returns whether the given email address *may* belong to an enterprise
  // domain; equivalent to extracting the domain and then checking
  // `MayBeEnterpriseDomain()`.
  static bool MayBeEnterpriseUserBasedOnEmail(const std::string& email);

  // Allows to register a domain that is recognized as non-enterprise for tests.
  // Note that `domain` needs to live until this method is invoked with nullptr.
  static void SetNonEnterpriseDomainForTesting(const char* domain);

  // After an AccountManagedStatusFinder is instantiated, the account type may
  // or may not be known immediately. The `async_callback` will only be run if
  // the account type was *not* known immediately, i.e. if `GetOutcome()` was
  // still `kPending` when the constructor returned. If the supplied `timeout`
  // value is equal to `base::TimeDelta::Max()` - `AccountManagedStatusFinder`
  // will wait for the managed status indefinitely (or until `IdentityManager`
  // is shut down); otherwise the management status will be set to `kTimeout`
  // after `timeout` time delay.
  AccountManagedStatusFinder(signin::IdentityManager* identity_manager,
                             const CoreAccountInfo& account,
                             base::OnceClosure async_callback,
                             base::TimeDelta timeout = base::TimeDelta::Max());
  ~AccountManagedStatusFinder() override;

  const CoreAccountInfo& GetAccountInfo() const { return account_; }

  Outcome GetOutcome() const { return outcome_; }

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnRefreshTokensLoaded() override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

#if BUILDFLAG(IS_ANDROID)
  // Implementation for JNI methods.
  void DestroyNativeObject(JNIEnv* env);
  jint GetOutcomeFromNativeObject(JNIEnv* env) const;
#endif

 private:
  void OnTimeoutReached();

  Outcome DetermineOutcome() const;

  void OutcomeDeterminedAsync(Outcome type);

  raw_ptr<signin::IdentityManager> identity_manager_;
  const CoreAccountInfo account_;
  bool ignore_persistent_auth_errors_ = true;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::OneShotTimer timeout_timer_;

  base::OnceClosure callback_;

  Outcome outcome_ = Outcome::kPending;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_
