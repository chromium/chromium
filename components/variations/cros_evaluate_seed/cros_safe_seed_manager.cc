// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/cros_safe_seed_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/variations/service/safe_seed_manager_base.h"
#include "components/variations/variations_seed_store.h"

namespace variations::cros_early_boot::evaluate_seed {

CrOSSafeSeedManager::CrOSSafeSeedManager(SeedType seed) : seed_(seed) {}

CrOSSafeSeedManager::~CrOSSafeSeedManager() = default;

SeedType CrOSSafeSeedManager::GetSeedType() const {
  return seed_;
}

std::optional<featured::SeedDetails> CrOSSafeSeedManager::GetUsedSeed() const {
  const std::optional<ActiveSeedState>& active_seed_state =
      GetActiveSeedState();
  if (!active_seed_state.has_value()) {
    return std::nullopt;
  }
  featured::SeedDetails details;
  details.set_locale(active_seed_state->client_filterable_state->locale);
  details.set_milestone(active_seed_state->seed_milestone);
  details.set_permanent_consistency_country(
      active_seed_state->client_filterable_state
          ->permanent_consistency_country);
  details.set_session_consistency_country(
      active_seed_state->client_filterable_state->session_consistency_country);
  details.set_signature(active_seed_state->base64_seed_signature);
  details.set_date(active_seed_state->client_filterable_state->reference_date
                       .ToDeltaSinceWindowsEpoch()
                       .InMilliseconds());
  details.set_fetch_time(
      active_seed_state->seed_fetch_time.ToDeltaSinceWindowsEpoch()
          .InMilliseconds());

  std::optional<std::string> base64_seed_data =
      VariationsSeedStore::SeedBytesToCompressedBase64Seed(
          active_seed_state->seed_data);
  if (!base64_seed_data.has_value()) {
    return std::nullopt;
  }
  details.set_b64_compressed_data(base64_seed_data.value());

  return details;
}

}  // namespace variations::cros_early_boot::evaluate_seed
