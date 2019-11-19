// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PSL_MATCHING_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PSL_MATCHING_HELPER_H_

#include <iosfwd>
#include <string>

#include "components/password_manager/core/browser/password_store.h"

class GURL;

namespace password_manager {

// Enum used for histogram tracking PSL Domain triggering.
// New entries should only be added to the end of the enum (before *_COUNT) so
// as to not disrupt existing data.
enum PSLDomainMatchMetric {
  PSL_DOMAIN_MATCH_NOT_USED = 0,
  PSL_DOMAIN_MATCH_NONE,
  PSL_DOMAIN_MATCH_FOUND,
  PSL_DOMAIN_MATCH_FOUND_FEDERATED,
  PSL_DOMAIN_MATCH_COUNT
};

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

// Returns true iff |form_signon_realm| designates a federated credential for
// |origin|. It doesn't check the port because |form_signon_realm| doesn't have
// it.
bool IsFederatedRealm(const std::string& form_signon_realm, const GURL& origin);

// Returns true iff |form_signon_realm| and |form_origin| designate a federated
// PSL matching credential for the |origin|.
bool IsFederatedPSLMatch(const std::string& form_signon_realm,
                         const GURL& form_origin,
                         const GURL& origin);
#endif

// Returns what type of match applies to |form| and |form_digest|.
MatchResult GetMatchResult(const autofill::PasswordForm& form,
                           const PasswordStore::FormDigest& form_digest);

// Two URLs are considered a Public Suffix Domain match if they have the same
// scheme, ports, and their registry controlled domains are equal. If one or
// both arguments do not describe valid URLs, returns false.
bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2);

// Two hosts are considered to belong to the same website when they share the
// registry-controlled domain part.
std::string GetRegistryControlledDomain(const GURL& signon_realm);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PSL_MATCHING_HELPER_H_
