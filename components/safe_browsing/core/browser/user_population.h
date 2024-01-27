// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_USER_POPULATION_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_USER_POPULATION_H_

#include <optional>

#include "base/feature_list.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

class PrefService;

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace safe_browsing {

// Returns the UserPopulation enum for the given prefs
ChromeUserPopulation::UserPopulation GetUserPopulationPref(PrefService* prefs);

// Get the status of each experiment in `experiments` and put it in the
// `finch_active_groups` field of `population`.
void GetExperimentStatus(const std::vector<const base::Feature*>& experiments,
                         ChromeUserPopulation* population);

// Creates a ChromeUserPopulation proto for the given state.
ChromeUserPopulation GetUserPopulation(
    // The below may be null.
    PrefService* prefs,
    bool is_incognito,
    bool is_history_sync_active,
    bool is_signed_in,
    bool is_under_advanced_protection,
    // The below may be null.
    const policy::BrowserPolicyConnector* browser_policy_connector,
    // The below state is optional, as it is not available in all
    // contexts/embedders.
    std::optional<size_t> num_profiles,
    std::optional<size_t> num_loaded_profiles,
    std::optional<size_t> num_open_profiles);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_USER_POPULATION_H_
