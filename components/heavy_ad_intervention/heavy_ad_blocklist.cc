// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"

#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"

namespace heavy_ad_intervention {

namespace {

const char kHostDurationHours[] = "host-duration-hours";
const char kHostThreshold[] = "host-threshold";
const char kHostsInMemory[] = "hosts-in-memory";

const char kTypeVersion[] = "type-version";

int GetBlocklistParamValue(const std::string& param, int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kHeavyAdPrivacyMitigations, param, default_value);
}

}  // namespace

HeavyAdBlocklist::HeavyAdBlocklist(
    std::unique_ptr<blocklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    blocklist::OptOutBlocklistDelegate* blocklist_delegate)
    : OptOutBlocklist(std::move(opt_out_store), clock, blocklist_delegate) {
  Init();
}

HeavyAdBlocklist::~HeavyAdBlocklist() = default;

bool HeavyAdBlocklist::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                              size_t* history,
                                              int* threshold) const {
  return false;
}

bool HeavyAdBlocklist::ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                                 size_t* history,
                                                 int* threshold) const {
  return false;
}

bool HeavyAdBlocklist::ShouldUseHostPolicy(base::TimeDelta* duration,
                                           size_t* history,
                                           int* threshold,
                                           size_t* max_hosts) const {
  const int kDefaultHostsInMemory = 50;
  const int kDefaultHostDurationHours = 24;
  const int kDefaultHostThreshold = 5;
  *max_hosts = GetBlocklistParamValue(kHostsInMemory, kDefaultHostsInMemory);
  *duration = base::Hours(
      GetBlocklistParamValue(kHostDurationHours, kDefaultHostDurationHours));
  *history = GetBlocklistParamValue(kHostThreshold, kDefaultHostThreshold);
  *threshold = GetBlocklistParamValue(kHostThreshold, kDefaultHostThreshold);
  return true;
}

bool HeavyAdBlocklist::ShouldUseTypePolicy(base::TimeDelta* duration,
                                           size_t* history,
                                           int* threshold) const {
  return false;
}

blocklist::BlocklistData::AllowedTypesAndVersions
HeavyAdBlocklist::GetAllowedTypes() const {
  return {{static_cast<int>(HeavyAdBlocklistType::kHeavyAdOnlyType),
           GetBlocklistParamValue(kTypeVersion, 0)}};
}

}  // namespace heavy_ad_intervention
