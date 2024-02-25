// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_COMMON_ORIGIN_TRIALS_PERSISTENCE_PROVIDER_H_
#define COMPONENTS_ORIGIN_TRIALS_COMMON_ORIGIN_TRIALS_PERSISTENCE_PROVIDER_H_

#include <vector>
#include "base/containers/flat_set.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "net/base/schemeful_site.h"

namespace url {
class Origin;
}

namespace origin_trials {

// Used for keying a mapping of sites (eTLD+1s) to origins with tokens persisted
// for them. `SiteKey` may change to some internal type, so embedders should
// treat it as an opaque, but serializable, value.
using SiteKey = net::SchemefulSite;
// A collection of persisted trial tokens that match a given SiteKey. Each entry
// in the collection is a pair, representing tokens persisted for a specific
// origin. Just an alias for user convenience.
using SiteOriginTrialTokens =
    std::vector<std::pair<url::Origin, base::flat_set<PersistedTrialToken>>>;

class OriginTrialsPersistenceProvider {
 public:
  virtual ~OriginTrialsPersistenceProvider() = default;

  // Return the list of persistent origin trial tokens that were previously
  // saved for |origin|.
  virtual base::flat_set<PersistedTrialToken> GetPersistentTrialTokens(
      const url::Origin& origin) = 0;

  // Return a vector of `origin` - `base::flat_set<PersistedTrialToken>` pairs
  // representing previously saved persistent trial tokens that may match
  // `origin`. Unlike GetPersistentTrialTokens(), the sets
  //  include tokens for all origins with the same `SiteKey` value as `origin`.
  //  Call sites are expected to filter the returned tokens to determine which
  //  actually match `origin` based on their respective `origin` and
  //  `match_subdomains` values.
  virtual SiteOriginTrialTokens GetPotentialPersistentTrialTokens(
      const url::Origin& origin) = 0;

  // Save the list of enabled trial tokens for |origin|.
  virtual void SavePersistentTrialTokens(
      const url::Origin& origin,
      const base::flat_set<PersistedTrialToken>& tokens) = 0;

  // Clear any stored trial tokens.
  virtual void ClearPersistedTokens() = 0;

  // Returns a key based on the given origin, which will be used to find all
  // tokens potentially relevant to it.
  static SiteKey GetSiteKey(const url::Origin& origin) {
    return net::SchemefulSite(origin);
  }
};

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_COMMON_ORIGIN_TRIALS_PERSISTENCE_PROVIDER_H_
