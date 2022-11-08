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

void TestPersistenceProvider::SavePersistentTrialTokens(
    const url::Origin& origin,
    const base::flat_set<origin_trials::PersistedTrialToken>& tokens) {
  if (tokens.empty()) {
    storage_.erase(origin);
  } else {
    storage_[origin] = tokens;
  }
}

void TestPersistenceProvider::ClearPersistedTokens() {
  storage_.clear();
}

}  // namespace origin_trials::test
