// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EVALUATE_SEED_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EVALUATE_SEED_H_

#include <stdio.h>

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/cros_evaluate_seed/cros_variations_field_trial_creator.h"
#include "components/variations/service/variations_field_trial_creator_base.h"
#include "components/variations/service/variations_service_client.h"

namespace variations::cros_early_boot::evaluate_seed {
inline constexpr char kSafeSeedSwitch[] = "use-safe-seed";
inline constexpr char kEnterpriseEnrolledSwitch[] = "enterprise-enrolled";
inline constexpr char kLocalStatePathSwitch[] = "local-state-path";

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

  base::FilePath GetVariationsSeedFileDir() override;

  bool IsEnterprise() override;

  // Multi-profile early-boot experiments are not supported.
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  version_info::Channel GetChannel() override;
};

struct SafeSeed {
  bool use_safe_seed = false;  // use the data in |seed_data| or ignore it?
  featured::SeedDetails seed_data;
};

// Read the safe seed data from |stream|, if and only if the command-line
// indicates that we should use the safe seed.
// Returns nullopt if reading the safe seed failed, but we wanted one.
// Otherwise, returns a SafeSeed struct, with the |use_safe_seed| field
// indicating whether to use the associated data in |seed_data|.
std::optional<SafeSeed> GetSafeSeedData(FILE* stream);

// Return a CrOSVariationsFieldTrialCreator for either the safe seed or the
// local-state-based seed, depending on whether |safe_seed_details| has a safe
// seed specified.
std::unique_ptr<CrOSVariationsFieldTrialCreator> GetFieldTrialCreator(
    PrefService* local_state,
    CrosVariationsServiceClient* client,
    const std::optional<featured::SeedDetails>& safe_seed_details);

typedef base::OnceCallback<std::unique_ptr<CrOSVariationsFieldTrialCreator>(
    PrefService* local_state,
    CrosVariationsServiceClient* client,
    const std::optional<featured::SeedDetails>& safe_seed_details)>
    GetCrOSVariationsFieldTrialCreator;

// Evaluate the seed state, writing serialized computed output to stdout.
// In most cases, this will read seed state from the local state file as
// specified in kLocalStatePathSwitch, defaulting to a common fallback,
// kDefaultLocalStatePath.
// If kSafeSeedSwitch is specified, read SeedDetails from |in_stream| for safe
// seed and associated data.
// Writes a serialized ComputedState proto to |out_stream|.
// Uses |GetFieldTrialCreator| to get a CrOSVariationsFieldTrialCreator.
// Return values are standard for main methods (EXIT_SUCCESS / EXIT_FAILURE).
int EvaluateSeedMain(FILE* in_stream, FILE* out_stream);

// |get_creator| is a callback that will be invoked instead of
// |GetFieldTrialCreator| to get a CrOSVariationsFieldTrialCreator. Otherwise,
// the same as EvaluateSeedMain.
int EvaluateSeedMain(FILE* in_stream,
                     FILE* out_stream,
                     GetCrOSVariationsFieldTrialCreator get_creator);

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EVALUATE_SEED_H_
