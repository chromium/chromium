// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_OUTCOME_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_OUTCOME_H_

namespace signin {

// The outcome of an account managed-ness check; see
// `AccountManagedStatusFinder`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// This enum is also used in Java.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.identitymanager
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AccountManagedStatusFinderOutcome
//
// LINT.IfChange(AccountManagedStatusFinderOutcome)
enum class AccountManagedStatusFinderOutcome {
  // The check isn't complete yet.
  kPending = 0,
  // An error happened, e.g. the account was removed from IdentityManager.
  kError = 1,
  // The account is a consumer (non-enterprise) account, split further into
  // Gmail accounts, account from other well-known consumer domains (i.e.
  // determined statically and synchronously), and accounts from other
  // domains. This distinction is mainly interesting for metrics.
  kConsumerGmail = 2,
  kConsumerWellKnown = 3,
  kConsumerNotWellKnown = 4,
  // The account is an @google.com enterprise account.
  kEnterpriseGoogleDotCom = 5,
  // The account is an enterprise account but *not* an @google.com one.
  kEnterprise = 6,
  // The timeout was reached before the management status could be decided.
  kTimeout = 7,

  kMaxValue = kTimeout
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountManagedStatusFinderOutcome)

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_OUTCOME_H_
