// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_SEED_STORE_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_SEED_STORE_H_

#include "base/time/time.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_store.h"

class PrefService;

namespace variations::cros_early_boot::evaluate_seed {

// VariationsSeedStore that uses a safe seed specific to early-boot ChromeOS.
//
// While early-boot experiments share a seed with non-early-boot experiments and
// use the same code to load them from |local_state|, they do *not* share a safe
// seed, since a seed could be safe for Chromium without being safe for
// early-boot ChromeOS.
class EarlyBootSeedStore : public VariationsSeedStore {
 public:
  // Construct an EarlyBootSeedStore, using |local_state| for the normal
  // seed and |safe_seed_details| (which may be nullopt if we are not in safe
  // seed mode) for the safe seed.
  EarlyBootSeedStore(
      PrefService* local_state,
      const absl::optional<featured::SeedDetails>& safe_seed_details);

  EarlyBootSeedStore(const EarlyBootSeedStore&) = delete;
  EarlyBootSeedStore& operator=(const EarlyBootSeedStore&) = delete;

  ~EarlyBootSeedStore() override;

  // Populate the given |seed| and |client_state| with the safe seed state as
  // specified in the constructor.
  // Unlike the base class version, DOES NOT modify local state or have any side
  // effects.
  bool LoadSafeSeed(VariationsSeed* seed,
                    ClientFilterableState* client_state) override;

  // Returns the time at which the safe seed was persisted to the platform-side
  // store.
  base::Time GetSafeSeedFetchTime() const override;

 private:
  const absl::optional<featured::SeedDetails> safe_seed_details_;
};

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_SEED_STORE_H_
