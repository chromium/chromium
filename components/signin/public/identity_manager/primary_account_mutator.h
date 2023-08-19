// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

namespace signin_metrics {
enum class ProfileSignout;
enum class SignoutDelete;
}  // namespace signin_metrics

struct CoreAccountId;

namespace signin {
enum class ConsentLevel;

// PrimaryAccountMutator is the interface to set and clear the primary account
// (see IdentityManager for more information).
//
// This interface has concrete implementations on platform that
// support changing the signed-in state during the lifetime of the application.
// On other platforms, there is no implementation, and no instance will be
// available at runtime (thus accessors may return null).
class PrimaryAccountMutator {
 public:
  // Error returned by SetPrimaryAccount().
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.identitymanager
  enum class PrimaryAccountError {
    // No error, the operation was successful.
    kNoError = 0,
    // Account info is empty.
    kAccountInfoEmpty = 1,
    // Sync consent was already set.
    kSyncConsentAlreadySet = 2,
    // Sign-in is disallowed.
    kSigninNotAllowed = 4,
    // The primary account cannot be modified.
    kPrimaryAccountChangeNotAllowed = 5,
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

  // For ConsentLevel::kSync -
  // Marks the account with `account_id` as the primary account, and returns
  // whether the operation succeeded or not. To succeed, this requires that:
  //    - the account is known by the IdentityManager.
  // On non-ChromeOS platforms, this additionally requires that:
  //    - setting the primary account is allowed,
  //    - the account username is allowed by policy,
  //    - there is not already a primary account set.
  // TODO(https://crbug.com/983124): Investigate adding all the extra
  // requirements on ChromeOS as well.
  //
  // For ConsentLevel::kSignin -
  // Sets the account with `account_id` as the unconsented primary account
  // (i.e. without implying browser sync consent). Requires that the account
  // is known by the IdentityManager. See README.md for details on the meaning
  // of "unconsented". Returns whether the operation succeeded or not.
  // On non-ChromeOS platforms, this additionally requires that:
  //    - setting the primary account is allowed,
  //    - there is not already a managed primary account set.
  //
  // The account state changes will be recorded in UMA, attributed to the
  // provided `access_point`.
  // TODO(crbug.com/1261772): Don't set a default `access_point`. All callsites
  //     should provide a valid value.
  // TODO(crbug.com/1462858): ConsentLevel::kSync is being migrated away from,
  //     please see ConsentLevel::kSync documentation before adding new calls
  //     with ConsentLevel::kSync. Also, update this documentation when the
  //     deprecation process advances.
  virtual PrimaryAccountError SetPrimaryAccount(
      const CoreAccountId& account_id,
      ConsentLevel consent_level,
      signin_metrics::AccessPoint access_point =
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN) = 0;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Revokes sync consent from the primary account. We distinguish the following
  // cases:
  // a. If transitioning from ConsentLevel::kSync to ConsentLevel::kSignin
  //    is supported (e.g. for DICE), then this method only revokes the sync
  //    consent and the primary account is left at ConsentLevel::kSignin
  //    level.
  // b. Otherwise this method revokes the sync consent and it also  clears the
  //    primary account and removes all other accounts via a call to
  //    ClearPrimaryAccount().
  //
  // Note: This method expects that the user already consented for sync.
  virtual void RevokeSyncConsent(
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) = 0;

  // Clears the primary account, removes all accounts and revokes the sync
  // consent. Returns true if the action was successful and false if there
  // was no primary account set.
  virtual bool ClearPrimaryAccount(
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) = 0;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_MUTATOR_H_
