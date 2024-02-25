// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/test/test_persistence_provider.h"

namespace origin_trials::test {

TestPersistenceProvider::TestPersistenceProvider() = default;

TestPersistenceProvider::~TestPersistenceProvider() = default;

base::flat_set<origin_trials::PersistedTrialToken>
TestPersistenceProvider::GetPersistentTrialTokens(const url::Origin& origin) {
  std::map<url::Origin,
           base::flat_set<origin_trials::PersistedTrialToken>>::const_iterator
      entry = storage_.find(origin);
  if (entry != storage_.end()) {
    return entry->second;
  }
  return {};
}

std::vector<std::pair<url::Origin, base::flat_set<PersistedTrialToken>>>
TestPersistenceProvider::GetPotentialPersistentTrialTokens(
    const url::Origin& origin) {
  std::vector<std::pair<url::Origin, base::flat_set<PersistedTrialToken>>>
      site_tokens;
  auto site_iter = sitekey_map_.find(GetSiteKey(origin));

  if (site_iter == sitekey_map_.end()) {
    return site_tokens;
  }

  for (const auto& token_origin : site_iter->second) {
    auto token_origin_iter = storage_.find(token_origin);
    if (token_origin_iter != storage_.end()) {
      site_tokens.emplace_back(token_origin, token_origin_iter->second);
    }
  }

  return site_tokens;
}

void TestPersistenceProvider::SavePersistentTrialTokens(
    const url::Origin& origin,
    const base::flat_set<origin_trials::PersistedTrialToken>& tokens) {
  if (tokens.empty()) {
    storage_.erase(origin);
    UpdateSiteToOriginsMap(origin, /*insert=*/false);
  } else {
    storage_[origin] = tokens;
    UpdateSiteToOriginsMap(origin, /*insert=*/true);
  }
}

void TestPersistenceProvider::ClearPersistedTokens() {
  storage_.clear();
}

void TestPersistenceProvider::UpdateSiteToOriginsMap(const url::Origin& origin,
                                                     bool insert) {
  // Removes `origin` from the set of origins for its SiteKey if `insert` is
  // false. If insert is true, Adds `origin` to the set of origins for its
  // SiteKey if `insert` is true, if it .
  SiteKey site_key = GetSiteKey(origin);
  auto find_it = sitekey_map_.find(site_key);
  bool site_mapped = (find_it != sitekey_map_.end());
  bool origin_mapped = (site_mapped && find_it->second.contains(origin));

  bool needs_create = insert && !site_mapped;
  bool needs_update = insert && (site_mapped && !origin_mapped);
  bool needs_delete = !insert && origin_mapped;

  if (needs_delete) {
    // There exists an origin set for `site_key` and `origin` needs to be
    // removed from it.
    find_it->second.erase(origin);

    if (find_it->second.empty()) {
      sitekey_map_.erase(find_it);
    }
  } else if (needs_update) {
    // There exists an origin set for `site_key` and `origin` needs to be
    // added to it.
    find_it->second.insert(origin);
  } else if (needs_create) {
    // There is not an existing origin set for `site_key`, but one needs to be
    // created (containing `origin`).
    sitekey_map_.insert_or_assign(find_it, site_key,
                                  base::flat_set<url::Origin>({origin}));
  }
}

}  // namespace origin_trials::test
