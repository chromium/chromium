// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_FEATURE_CONFIG_EVENT_STORAGE_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_FEATURE_CONFIG_EVENT_STORAGE_VALIDATOR_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "components/feature_engagement/internal/event_storage_validator.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"

namespace feature_engagement {
class Configuration;
struct EventConfig;
struct FeatureConfig;
struct GroupConfig;

// A EventStorageValidator that uses the FeatureConfig and GroupConfig as the
// source of truth.
class FeatureConfigEventStorageValidator : public EventStorageValidator {
 public:
  FeatureConfigEventStorageValidator();

  FeatureConfigEventStorageValidator(
      const FeatureConfigEventStorageValidator&) = delete;
  FeatureConfigEventStorageValidator& operator=(
      const FeatureConfigEventStorageValidator&) = delete;

  ~FeatureConfigEventStorageValidator() override;

  // EventStorageValidator implementation.
  bool ShouldStore(const std::string& event_name) const override;
  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override;

  // Set up internal configuration required for the given |features| and
  // |groups|.
  void InitializeFeatures(const FeatureVector& features,
                          const GroupVector& groups,
                          const Configuration& configuration);

  // Resets the full state of this EventStorageValidator. After calling this
  // method it is valid to call InitializeFeatures() again.
  void ClearForTesting();

 private:
  // Updates the internal configuration with conditions from the given
  // |feature_config|.
  void InitializeFeatureConfig(const FeatureConfig& feature_config);

  // Updates the internal configuration with conditions from the given
  // |group_config|.
  void InitializeGroupConfig(const GroupConfig& group_config);

  // Updates the internal configuration with conditions from the given
  // |event_config|.
  void InitializeEventConfig(const EventConfig& event_config);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Updates the internal allowed prefixes set.
  void InitializeEventPrefixes(const Configuration& configuration);
#endif

  // Contains an entry for each of the events that any EventConfig required to
  // be stored.
  std::unordered_set<std::string> should_store_event_names_;

  // Contains an allowed list of prefixes of the event names that any
  // ConfigurationProvider required to be stored.
  std::unordered_set<std::string> should_store_event_name_prefixes_;

  // Contains the longest time to store each event across all EventConfigs,
  // as a number of days.
  std::unordered_map<std::string, uint32_t> longest_storage_times_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_FEATURE_CONFIG_EVENT_STORAGE_VALIDATOR_H_
