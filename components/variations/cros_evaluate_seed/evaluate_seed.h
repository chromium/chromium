// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EVALUATE_SEED_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EVALUATE_SEED_H_

#include <memory>
#include <string>

#include <stdio.h>

#include "base/command_line.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/service/variations_service_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations::cros_early_boot::evaluate_seed {

class CrosVariationsServiceClient : public VariationsServiceClient {
 public:
  CrosVariationsServiceClient() = default;

  CrosVariationsServiceClient(const CrosVariationsServiceClient&) = delete;
  CrosVariationsServiceClient& operator=(const CrosVariationsServiceClient&) =
      delete;

  base::Version GetVersionForSimulation() override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;

  bool OverridesRestrictParameter(std::string* parameter) override;

  bool IsEnterprise() override;

  // Multi-profile early-boot experiments are not supported.
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  version_info::Channel GetChannel() override;
};

// Retrieve a ClientFilterableState struct based on process's command-line.
std::unique_ptr<ClientFilterableState> GetClientFilterableState();

struct SafeSeed {
  bool use_safe_seed = false;
  featured::SeedDetails seed_data;
};

// Read the safe seed data from |stream|, if and only if the command-line
// indicates that we should use the safe seed.
absl::optional<SafeSeed> GetSafeSeedData(FILE* stream);

// Evaluate the seed state, writing serialized computed output to stdout.
// Reads a proto from |stream| for data like safe seed.
// Return values are standard for main methods (EXIT_SUCCESS / EXIT_FAILURE).
int EvaluateSeedMain(FILE* stream);

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EVALUATE_SEED_H_
