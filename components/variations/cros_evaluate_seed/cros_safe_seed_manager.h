// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_CROS_SAFE_SEED_MANAGER_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_CROS_SAFE_SEED_MANAGER_H_

#include <memory>
#include <string>

#include "components/variations/client_filterable_state.h"
#include "components/variations/service/safe_seed_manager_interface.h"

class PrefRegistrySimple;

namespace variations::cros_early_boot::evaluate_seed {

class CrOSSafeSeedManager : public SafeSeedManagerInterface {
 public:
  // Creates a CrOSSafeSeedManager used to determine the what type of seed to
  // use on CrOS devices.
  explicit CrOSSafeSeedManager(SeedType seed);

  CrOSSafeSeedManager(const CrOSSafeSeedManager&) = delete;
  CrOSSafeSeedManager& operator=(const CrOSSafeSeedManager&) = delete;

  ~CrOSSafeSeedManager() override;

  // Empty implementation since we do not want to modify Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry) {}

  // Returns the type of seed the client should use. Featured will determine the
  // seed and pass it to evaluate_seed.
  SeedType GetSeedType() const override;

  // Empty implementation since we do not need to set the active seed.
  void SetActiveSeedState(
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      int seed_milestone,
      std::unique_ptr<ClientFilterableState> client_filterable_state,
      base::Time seed_fetch_time) override {}

  // Empty implementation since we do not fetch the seed from evaluate_seed.
  void RecordFetchStarted() override {}

  // Empty implementation since we do not fetch the seed from evaluate_seed.
  void RecordSuccessfulFetch(VariationsSeedStore* seed_store) override {}

 private:
  SeedType seed_;
};

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_CROS_SAFE_SEED_MANAGER_H_
