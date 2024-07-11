// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_TEST_TEST_PERSISTENCE_PROVIDER_H_
#define COMPONENTS_ORIGIN_TRIALS_TEST_TEST_PERSISTENCE_PROVIDER_H_

#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "components/origin_trials/common/origin_trials_persistence_provider.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "url/origin.h"

namespace origin_trials::test {

// Class that provides in-memory implementation of
// OriginTrialsPersistenceProvider to be used for testing.
class TestPersistenceProvider : public OriginTrialsPersistenceProvider {
 public:
  TestPersistenceProvider();
  TestPersistenceProvider(const TestPersistenceProvider&) = delete;
  TestPersistenceProvider(const TestPersistenceProvider&&) = delete;

  ~TestPersistenceProvider() override;

  // OriginTrialsPersistenceProvider
  base::flat_set<origin_trials::PersistedTrialToken> GetPersistentTrialTokens(
      const url::Origin& origin) override;
  SiteOriginTrialTokens GetPotentialPersistentTrialTokens(
      const url::Origin& origin) override;
  void SavePersistentTrialTokens(
      const url::Origin& origin,
      const base::flat_set<origin_trials::PersistedTrialToken>& tokens)
      override;
  void ClearPersistedTokens() override;

 private:
  std::map<url::Origin, base::flat_set<origin_trials::PersistedTrialToken>>
      storage_;
  std::map<SiteKey, base::flat_set<url::Origin>> sitekey_map_;

  void UpdateSiteToOriginsMap(const url::Origin& origin, bool insert);
};

}  // namespace origin_trials::test

#endif  // COMPONENTS_ORIGIN_TRIALS_TEST_TEST_PERSISTENCE_PROVIDER_H_
