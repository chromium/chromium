// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_LOCAL_STATE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_LOCAL_STATE_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/variations/variations_safe_seed_store.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

// Implementation of VariationsSafeSeedStore that uses local state and the
// standard prefs from it.
class COMPONENT_EXPORT(VARIATIONS) VariationsSafeSeedStoreLocalState
    : public VariationsSafeSeedStore {
 public:
  // |local_state| should generally be the same value that VariationsSeedStore
  // uses.
  explicit VariationsSafeSeedStoreLocalState(PrefService* local_state);

  VariationsSafeSeedStoreLocalState(const VariationsSafeSeedStoreLocalState&) =
      delete;
  VariationsSafeSeedStoreLocalState& operator=(
      const VariationsSafeSeedStoreLocalState&) = delete;

  ~VariationsSafeSeedStoreLocalState() override;

  // VariationsSafeSeedStore overrides.
  base::Time GetFetchTime() const override;
  void SetFetchTime(const base::Time& fetch_time) override;

  int GetMilestone() const override;
  void SetMilestone(int milestone) override;

  base::Time GetTimeForStudyDateChecks() const override;
  void SetTimeForStudyDateChecks(const base::Time& safe_seed_time) override;

  std::string GetCompressedSeed() const override;
  void SetCompressedSeed(const std::string& safe_compressed) override;

  std::string GetSignature() const override;
  void SetSignature(const std::string& safe_seed_signature) override;

  std::string GetLocale() const override;
  void SetLocale(const std::string& locale) override;

  std::string GetPermanentConsistencyCountry() const override;
  void SetPermanentConsistencyCountry(
      const std::string& permanent_consistency_country) override;

  std::string GetSessionConsistencyCountry() const override;
  void SetSessionConsistencyCountry(
      const std::string& session_consistency_country) override;

  void ClearState() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Local State accessor, which should be the same as the one in
  // VariationsSeedStore.
  raw_ptr<PrefService> local_state_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_LOCAL_STATE_H_
