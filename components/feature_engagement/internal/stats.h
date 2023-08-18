// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_STATS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_STATS_H_

#include <string>
#include <vector>

#include "components/feature_engagement/internal/condition_validator.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {
namespace stats {

// Enum used in the metrics to record the result when in-product help UI is
// going to be triggered.
// Most of the fields maps to |ConditionValidator::Result|.
// The failure reasons are not mutually exclusive.
// Out-dated entries shouldn't be deleted but marked as obselete.
// Keep this synced with the enum in //tools/metrics/histograms/enums.xml.
enum class TriggerHelpUIResult {
  // The help UI is triggered.
  SUCCESS = 0,

  // The help UI is not triggered.
  FAILURE = 1,

  // Event model is not ready.
  FAILURE_EVENT_MODEL_NOT_READY = 2,

  // Some other help UI is currently showing.
  FAILURE_CURRENTLY_SHOWING = 3,

  // The feature is disabled.
  FAILURE_FEATURE_DISABLED = 4,

  // Configuration can not be parsed.
  FAILURE_CONFIG_INVALID = 5,

  // Used event precondition is not satisfied.
  FAILURE_USED_PRECONDITION_UNMET = 6,

  // Trigger event precondition is not satisfied.
  FAILURE_TRIGGER_PRECONDITION_UNMET = 7,

  // Other event precondition is not satisfied.
  FAILURE_OTHER_PRECONDITION_UNMET = 8,

  // Session rate does not meet the requirement.
  FAILURE_SESSION_RATE = 9,

  // Availability model is not ready.
  FAILURE_AVAILABILITY_MODEL_NOT_READY = 10,

  // Availability precondition is not satisfied.
  FAILURE_AVAILABILITY_PRECONDITION_UNMET = 11,

  // Same as |SUCCESS|, but feature configuration was set to tracking only.
  SUCCESS_TRACKING_ONLY = 12,

  // Display of help UI is locked.
  FAILURE_DISPLAY_LOCK = 13,

  // Groups conditions are not satisfied.
  FAILURE_GROUPS_PRECONDITION_UNMET = 14,

  // Last entry for the enum.
  COUNT = 15,
};

// Used in metrics to track database states. Each type will match to a suffix
// in the histograms to identify the database.
enum class StoreType {
  // Events store.
  EVENTS_STORE = 0,

  // Availability store.
  AVAILABILITY_STORE = 1,
};

// Helper function that converts a store type to histogram suffix string.
std::string ToDbHistogramSuffix(StoreType type);

// Records the feature engagement events. Used event will be tracked
// separately.
void RecordNotifyEvent(const std::string& event,
                       const Configuration* config,
                       bool is_model_ready);

// Records user action and the result histogram when in-product help will be
// shown to the user.
void RecordShouldTriggerHelpUI(const base::Feature& feature,
                               const FeatureConfig& feature_config,
                               const ConditionValidator::Result& result);

// Records when the user dismisses the in-product help UI.
void RecordUserDismiss();

// Records when the user dismisses or snoozes the snoozable in-product help UI.
void RecordUserSnoozeAction(Tracker::SnoozeAction snooze_action);

// Records the result of database updates.
void RecordDbUpdate(bool success, StoreType type);

// Record database init.
void RecordDbInitEvent(bool success, StoreType type);

// Records events database load event.
void RecordEventDbLoadEvent(bool success, const std::vector<Event>& events);

// Records availability database load event.
void RecordAvailabilityDbLoadEvent(bool success);

}  // namespace stats
}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_STATS_H_
