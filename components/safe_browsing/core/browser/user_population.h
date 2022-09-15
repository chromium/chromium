// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_USER_POPULATION_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_USER_POPULATION_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace safe_browsing {

// Returns the UserPopulation enum for the given prefs
ChromeUserPopulation::UserPopulation GetUserPopulationPref(PrefService* prefs);

// Creates a ChromeUserPopulation proto for the given state.
ChromeUserPopulation GetUserPopulation(
    // The below may be null.
    PrefService* prefs,
    bool is_incognito,
    bool is_history_sync_enabled,
    bool is_signed_in,
    bool is_under_advanced_protection,
    // The below may be null.
    const policy::BrowserPolicyConnector* browser_policy_connector,
    // The below state is optional, as it is not available in all
    // contexts/embedders.
    absl::optional<size_t> num_profiles,
    absl::optional<size_t> num_loaded_profiles,
    absl::optional<size_t> num_open_profiles);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_USER_POPULATION_H_
