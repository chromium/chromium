// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/reuse_check_utility.h"

#include <unordered_map>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace password_manager {

namespace {

// Extracts affiliation information from password grouping.
std::map<std::string, int> MapSignonRelamsToAffiliationGroups(
    const std::vector<AffiliatedGroup>& groups) {
  std::map<std::string, int> signon_realm_to_group;
  for (size_t i = 0; i < groups.size(); i++) {
    for (const auto& credential : groups[i].GetCredentials()) {
      for (const auto& facet : credential.facets) {
        signon_realm_to_group[facet.signon_realm] = i;
      }
    }
  }
  return signon_realm_to_group;
}

bool AllCredentialsBelongToSameGroup(
    const std::vector<const CredentialUIEntry*>& credentials,
    const std::map<std::string, int>& signon_realm_to_group) {
  std::set<int> group_ids;
  for (auto* const credential : credentials) {
    auto it = signon_realm_to_group.find(credential->GetFirstSignonRealm());
    if (it != signon_realm_to_group.end()) {
      group_ids.insert(it->second);
    }
  }
  return group_ids.size() == 1;
}

// Username are considered equivalent if they have the same normalized format.
// Empty usernames are skipped.
bool AllUsernameAreEquivalent(
    const std::vector<const CredentialUIEntry*>& credentials) {
  std::set<std::u16string> normalized_usernames;
  for (auto* const credential : credentials) {
    // Empty usernames should be skipped from comparison, because:
    // - If we consider them equal to everything, it breaks our equivalence
    // transitivity.
    // - If we don't consider them equal to other usernames, we won't hide reuse
    // issue for the case of <"user", "pwd"> and <"", "pwd">.
    if (credential->username.empty()) {
      continue;
    }

    normalized_usernames.insert(base::ToLowerASCII(credential->username));
  }
  return normalized_usernames.size() == 1;
}

bool HasOnlyAndroidApps(const CredentialUIEntry* credential) {
  return base::ranges::all_of(credential->facets, [](const auto& facet) {
    return affiliations::IsValidAndroidFacetURI(facet.signon_realm);
  });
}

bool IsMainDomainEqual(const std::set<std::string>& signon_realms) {
  std::set<std::string> domain_parts;
  for (const auto& signon_realm : signon_realms) {
    domain_parts.insert(net::registry_controlled_domains::GetDomainAndRegistry(
        GURL(signon_realm),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  return domain_parts.size() == 1;
}

bool AllDomainsAreEquivalent(
    const std::vector<const CredentialUIEntry*>& credentials) {
  std::set<std::string> signon_realms;
  for (auto* const credential : credentials) {
    // We don't have any good heuristics for grouping Android apps except for
    // Affiliations. This means that:
    // - If there's at least one CredentialUIEntry consisting of only Android
    // apps, we can't group it any further, so the heuristic is aborted.
    // - Otherwise all Android apps are filtered out (since they're connected
    // with some websites) and remaining websites are compared for equality.
    if (HasOnlyAndroidApps(credential)) {
      return false;
    }

    for (const auto& facet : credential->facets) {
      if (affiliations::IsValidAndroidFacetURI(facet.signon_realm)) {
        continue;
      }

      signon_realms.insert(facet.signon_realm);
    }
  }
  // TODO(crbug.com/40252723): Check additionally for local networks.
  return signon_realms.size() == 1 || IsMainDomainEqual(signon_realms);
}

}  // namespace

base::flat_set<std::u16string> BulkReuseCheck(
    const std::vector<CredentialUIEntry>& credentials,
    const std::vector<AffiliatedGroup>& groups) {
  std::unordered_map<std::u16string, std::vector<const CredentialUIEntry*>>
      password_to_credentials;

  for (const auto& credential : credentials) {
    password_to_credentials[credential.password].push_back(&credential);
  }

  base::flat_set<std::u16string> reused_passwords;
  auto signon_realm_to_group = MapSignonRelamsToAffiliationGroups(groups);

  for (const auto& [password, matching_credentials] : password_to_credentials) {
    // Skip password if it's used for a single credential.
    if (matching_credentials.size() == 1) {
      continue;
    }

    // Password reuse within one affiliated group is ignored.
    if (AllCredentialsBelongToSameGroup(matching_credentials,
                                        signon_realm_to_group)) {
      continue;
    }

    if (AllUsernameAreEquivalent(matching_credentials) &&
        AllDomainsAreEquivalent(matching_credentials)) {
      continue;
    }
    reused_passwords.insert(password);
  }

  base::UmaHistogramCounts1000("PasswordManager.ReuseCheck.CheckedPasswords",
                               password_to_credentials.size());
  base::UmaHistogramCounts1000("PasswordManager.ReuseCheck.ReusedPasswords",
                               reused_passwords.size());
  return reused_passwords;
}

}  // namespace password_manager
