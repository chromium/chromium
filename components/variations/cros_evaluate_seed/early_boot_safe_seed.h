// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_SAFE_SEED_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_SAFE_SEED_H_

#include "base/time/time.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/variations/variations_safe_seed_store.h"

namespace variations::cros_early_boot::evaluate_seed {

// Read-only implementation of VariationsSafeSeedStore that uses the
// specified |safe_seed_details|.
class EarlyBootSafeSeed : public VariationsSafeSeedStore {
 public:
  explicit EarlyBootSafeSeed(const featured::SeedDetails& safe_seed_details);

  EarlyBootSafeSeed(const EarlyBootSafeSeed&) = delete;
  EarlyBootSafeSeed& operator=(const EarlyBootSafeSeed&) = delete;

  ~EarlyBootSafeSeed() override;

  // VariationsSafeSeedStore overrides.
  // None of the Set* or Clear* methods will do anything; evaluate_seed should
  // be read-only.
  // Instead, we will only modify the safe seed state when chrome has started
  // and confirmed that the seed works.
  // However, it is not per se an *error* to call the mutator methods here, we
  // just want to ignore these calls since we cannot actually determine if the
  // seed was safe while evaluate_seed is running.
  // The reason for this is that, in part of evaluating a seed, the variations
  // code will generally mark a prior seed as safe, but in this case we do not
  // yet know if the seed is safe until after ash-chrome has started. So, when
  // running evaluate_seed, we should ignore updates to the safe seed.
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

 private:
  // Details of the safe seed to use for early boot.
  const featured::SeedDetails safe_seed_details_;
};

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_SAFE_SEED_H_
