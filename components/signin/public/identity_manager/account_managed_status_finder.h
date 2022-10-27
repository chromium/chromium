// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_

#include <string>

namespace signin {

// Helper class to determine if a given account is a managed (aka enterprise)
// account.
class AccountManagedStatusFinder {
 public:
  // Check whether the given account is known to be non-enterprise. Domains such
  // as gmail.com and googlemail.com are known to not be managed. Also returns
  // true if the username is empty or not a valid email address.
  // Note that this is accurate in only one direction: If it returns true, the
  // account is definitely non-enterprise. But if it returns false, it may or
  // may not be an enterprise account.
  static bool IsNonEnterpriseUser(const std::string& email);

  // Allows to register a domain that is recognized as non-enterprise for tests.
  // Note that `domain` needs to live until this method is invoked with nullptr.
  static void SetNonEnterpriseDomainForTesting(const char* domain);

  // TODO(crbug.com/1378553): Add the actual (non-heuristic) implementation.
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_MANAGED_STATUS_FINDER_H_
