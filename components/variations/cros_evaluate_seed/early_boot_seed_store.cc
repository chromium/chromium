// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_seed_store.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "components/variations/client_filterable_state.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations::cros_early_boot::evaluate_seed {

EarlyBootSeedStore::EarlyBootSeedStore(
    PrefService* local_state,
    const absl::optional<featured::SeedDetails>& safe_seed_details)
    : VariationsSeedStore(local_state), safe_seed_details_(safe_seed_details) {}

EarlyBootSeedStore::~EarlyBootSeedStore() = default;

bool EarlyBootSeedStore::LoadSafeSeed(VariationsSeed* seed,
                                      ClientFilterableState* client_state) {
  // We require that evaluate_seed's command line specified a safe seed in order
  // to use the safe seed.
  CHECK(safe_seed_details_.has_value());
  absl::optional<VerifySignatureResult> verify_signature_result;
  if (VerifyAndParseSeed(seed, safe_seed_details_->compressed_data(),
                         safe_seed_details_->signature(),
                         &verify_signature_result) !=
      LoadSeedResult::kSuccess) {
    return false;
  }

  client_state->reference_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(safe_seed_details_->date()));
  client_state->locale = safe_seed_details_->locale();
  client_state->permanent_consistency_country =
      safe_seed_details_->permanent_consistency_country();
  client_state->session_consistency_country =
      safe_seed_details_->session_consistency_country();

  return true;
}

base::Time EarlyBootSeedStore::GetSafeSeedFetchTime() const {
  // We require that evaluate_seed's command line specified a safe seed in order
  // to use the safe seed.
  CHECK(safe_seed_details_.has_value());
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(safe_seed_details_->fetch_time()));
}

}  // namespace variations::cros_early_boot::evaluate_seed
