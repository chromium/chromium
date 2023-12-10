// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PSL_MATCHING_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PSL_MATCHING_HELPER_H_

#include <iosfwd>
#include <string>

#include "components/password_manager/core/browser/password_store/password_store_interface.h"

class GURL;

namespace password_manager {

enum class MatchResult {
  NO_MATCH,
  EXACT_MATCH,
  PSL_MATCH,
  FEDERATED_MATCH,
  FEDERATED_PSL_MATCH,
};

#if defined(UNIT_TEST)
std::ostream& operator<<(std::ostream& out, MatchResult result);

// These functions are used in production internally but exposed for testing.

// Returns true iff |form_signon_realm| and |form_origin| designate a federated
// PSL matching credential for the |origin|.
bool IsFederatedPSLMatch(const std::string& form_signon_realm,
                         const GURL& form_origin,
                         const GURL& origin);
#endif

// Returns true iff |form_signon_realm| designates a federated credential for
// |origin|. It doesn't check the port because |form_signon_realm| doesn't have
// it.
bool IsFederatedRealm(const std::string& form_signon_realm, const GURL& origin);

// Returns what type of match applies to |form| and |form_digest|.
MatchResult GetMatchResult(const PasswordForm& form,
                           const PasswordFormDigest& form_digest);

// Two URLs are considered a Public Suffix Domain match if they have the same
// scheme, ports, and their registry controlled domains are equal. If one or
// both arguments do not describe valid URLs, returns false.
bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2);

// Two hosts are considered to belong to the same website when they share the
// registry-controlled domain part.
std::string GetRegistryControlledDomain(const GURL& signon_realm);

// Returns the regular expression to match |signon_realm| when Public Suffix
// Domain matching is enabled. Used to retrieve logins from LoginsDatabase with
// 'WHERE signon_realm REGEX x' query and to verify logins retrieved from the
// downstream PasswordStoreBackend implementation with C++ Regex matcher.
std::string GetRegexForPSLMatching(const std::string& signon_realm);

// Returns the regular expression to match |form| when Public Suffix Domain &
// federated matching is enabled. Used to retrieve logins from LoginsDatabase
// with 'WHERE signon_realm REGEX x' query and to verify logins retrieved from
// the downstream PasswordStoreBackend implementation with C++ Regex matcher.
std::string GetRegexForPSLFederatedMatching(const std::string& signon_realm);

// Returns the expression to match |url| when federated matching is enabled.
// Used to retrieve logins from LoginsDatabase with 'WHERE signon_realm LIKE x'
// query and to verify logins retrieved from the downstream PasswordStoreBackend
// implementation with C++ Regex matcher.
std::string GetExpressionForFederatedMatching(const GURL& url);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PSL_MATCHING_HELPER_H_
