// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/feature_config_event_storage_validator.h"

#include <unordered_map>
#include <unordered_set>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

FeatureConfigEventStorageValidator::FeatureConfigEventStorageValidator() =
    default;

FeatureConfigEventStorageValidator::~FeatureConfigEventStorageValidator() =
    default;

bool FeatureConfigEventStorageValidator::ShouldStore(
    const std::string& event_name) const {
  for (const auto& prefix : should_store_event_name_prefixes_) {
    CHECK(!prefix.empty());

    if (base::StartsWith(event_name, prefix)) {
      return true;
    }
  }

  return should_store_event_names_.find(event_name) !=
         should_store_event_names_.end();
}

bool FeatureConfigEventStorageValidator::ShouldKeep(
    const std::string& event_name,
    uint32_t event_day,
    uint32_t current_day) const {
  for (const auto& prefix : should_store_event_name_prefixes_) {
    CHECK(!prefix.empty());

    if (base::StartsWith(event_name, prefix)) {
      return true;
    }
  }

  // Should not keep events that will happen in the future.
  if (event_day > current_day)
    return false;

  // If no feature configuration mentioned the event, it should not be kept.
  auto it = longest_storage_times_.find(event_name);
  if (it == longest_storage_times_.end())
    return false;

  // Too old events should not be kept.
  // Storage time of N=0:  Nothing should be kept.
  // Storage time of N=1:  |current_day| should be kept.
  // Storage time of N=2+: |current_day| plus |N-1| more days should be kept.
  uint32_t longest_storage_time = it->second;
  uint32_t age = current_day - event_day;
  if (longest_storage_time <= age)
    return false;

  return true;
}

void FeatureConfigEventStorageValidator::InitializeFeatures(
    const FeatureVector& features,
    const GroupVector& groups,
    const Configuration& configuration) {
  for (const auto* feature : features) {
    if (!base::FeatureList::IsEnabled(*feature))
      continue;

    InitializeFeatureConfig(configuration.GetFeatureConfig(*feature));
  }

  for (const auto* group : groups) {
    if (!base::FeatureList::IsEnabled(*group)) {
      continue;
    }

    InitializeGroupConfig(configuration.GetGroupConfig(*group));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  InitializeEventPrefixes(configuration);
#endif
}

void FeatureConfigEventStorageValidator::ClearForTesting() {
  should_store_event_names_.clear();
  longest_storage_times_.clear();
  should_store_event_name_prefixes_.clear();
}

void FeatureConfigEventStorageValidator::InitializeFeatureConfig(
    const FeatureConfig& feature_config) {
  InitializeEventConfig(feature_config.used);
  InitializeEventConfig(feature_config.trigger);

  for (const auto& event_config : feature_config.event_configs)
    InitializeEventConfig(event_config);
}

void FeatureConfigEventStorageValidator::InitializeGroupConfig(
    const GroupConfig& group_config) {
  InitializeEventConfig(group_config.trigger);

  for (const auto& event_config : group_config.event_configs) {
    InitializeEventConfig(event_config);
  }
}

void FeatureConfigEventStorageValidator::InitializeEventConfig(
    const EventConfig& event_config) {
  // Minimum storage time is 1 day.
  if (event_config.storage < 1u)
    return;

  // When minimum storage time is met, new events should always be stored.
  should_store_event_names_.insert(event_config.name);

  // Track the longest time any configuration wants to store a particular event.
  uint32_t current_longest_time = longest_storage_times_[event_config.name];
  if (event_config.storage > current_longest_time)
    longest_storage_times_[event_config.name] = event_config.storage;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FeatureConfigEventStorageValidator::InitializeEventPrefixes(
    const Configuration& configuration) {
  const auto& prefixes = configuration.GetRegisteredAllowedEventPrefixes();
  should_store_event_name_prefixes_.insert(prefixes.begin(), prefixes.end());
}
#endif

}  // namespace feature_engagement
