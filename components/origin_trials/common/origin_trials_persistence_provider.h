// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_COMMON_ORIGIN_TRIALS_PERSISTENCE_PROVIDER_H_
#define COMPONENTS_ORIGIN_TRIALS_COMMON_ORIGIN_TRIALS_PERSISTENCE_PROVIDER_H_

#include "base/containers/flat_set.h"
#include "components/origin_trials/common/persisted_trial_token.h"

namespace url {
class Origin;
}

namespace origin_trials {

class OriginTrialsPersistenceProvider {
 public:
  virtual ~OriginTrialsPersistenceProvider() = default;

  // Return the list of persistent origin trial tokens that were previously
  // saved for |origin|.
  virtual base::flat_set<PersistedTrialToken> GetPersistentTrialTokens(
      const url::Origin& origin) = 0;

  // Save the list of enabled trial tokens for |origin|.
  virtual void SavePersistentTrialTokens(
      const url::Origin& origin,
      const base::flat_set<PersistedTrialToken>& tokens) = 0;

  // Clear any stored trial tokens.
  virtual void ClearPersistedTokens() = 0;
};

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_COMMON_ORIGIN_TRIALS_PERSISTENCE_PROVIDER_H_
