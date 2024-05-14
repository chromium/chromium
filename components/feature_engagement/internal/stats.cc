// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/stats.h"

#include <string>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_engagement {
namespace stats {
namespace {

// Histogram suffixes for database metrics, must match the ones in
// histograms.xml.
const char kEventStoreSuffix[] = "EventStore";
const char kAvailabilityStoreSuffix[] = "AvailabilityStore";

// A shadow histogram across all features. Also the base name for the suffix
// based feature specific histograms; for example for IPH_MyFun, it would be:
// InProductHelp.ShouldTriggerHelpUI.IPH_MyFun.
const char kShouldTriggerHelpUIHistogram[] =
    "InProductHelp.ShouldTriggerHelpUI";

// Helper function to log a TriggerHelpUIResult.
void LogTriggerHelpUIResult(const std::string& name,
                            TriggerHelpUIResult result) {
  // Must not use histograms macros here because we pass in the histogram name.
  base::UmaHistogramEnumeration(name, result, TriggerHelpUIResult::COUNT);
  base::UmaHistogramEnumeration(kShouldTriggerHelpUIHistogram, result,
                                TriggerHelpUIResult::COUNT);
}

}  // namespace

std::string ToDbHistogramSuffix(StoreType type) {
  switch (type) {
    case StoreType::EVENTS_STORE:
      return std::string(kEventStoreSuffix);
    case StoreType::AVAILABILITY_STORE:
      return std::string(kAvailabilityStoreSuffix);
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

void RecordNotifyEvent(const std::string& event_name,
                       const Configuration* config,
                       bool is_model_ready) {
  DCHECK(!event_name.empty());
  DCHECK(config);

  // Find which feature this event belongs to.
  const Configuration::ConfigMap& features =
      config->GetRegisteredFeatureConfigs();
  std::string feature_name;
  for (const auto& element : features) {
    const std::string fname = element.first;
    const FeatureConfig& feature_config = element.second;

    // Track used event separately.
    if (feature_config.used.name == event_name) {
      feature_name = fname;
      DCHECK(!feature_name.empty());
      std::string used_event_action = "InProductHelp.NotifyUsedEvent.";
      used_event_action.append(feature_name);
      base::RecordComputedAction(used_event_action);
      break;
    }

    // Find if the |event_name| matches any configuration.
    for (const auto& event : feature_config.event_configs) {
      if (event.name == event_name) {
        feature_name = fname;
        break;
      }
    }
    if (feature_config.trigger.name == event_name) {
      feature_name = fname;
      break;
    }
  }

  // Do nothing if no events in the configuration matches the |event_name|.
  if (feature_name.empty())
    return;

  std::string event_action = "InProductHelp.NotifyEvent.";
  event_action.append(feature_name);
  base::RecordComputedAction(event_action);

  std::string event_histogram = "InProductHelp.NotifyEventReadyState.";
  event_histogram.append(feature_name);
  base::UmaHistogramBoolean(event_histogram, is_model_ready);
}

void RecordShouldTriggerHelpUI(const base::Feature& feature,
                               const FeatureConfig& feature_config,
                               const ConditionValidator::Result& result) {
  // Records the user action.
  std::string name = std::string(kShouldTriggerHelpUIHistogram)
                         .append(".")
                         .append(feature.name);
  base::RecordComputedAction(name);

  // Total count histogram, used to compute the percentage of each failure type,
  // in addition to a user action for whether the result was to trigger or not.
  if (result.NoErrors()) {
    LogTriggerHelpUIResult(name,
                           feature_config.tracking_only
                               ? TriggerHelpUIResult::SUCCESS_TRACKING_ONLY
                               : TriggerHelpUIResult::SUCCESS);
    std::string action_name = "InProductHelp.ShouldTriggerHelpUIResult.";
    action_name.append(feature_config.tracking_only ? "WouldHaveTriggered"
                                                    : "Triggered");
    action_name.append(".");
    action_name.append(feature.name);
    base::RecordComputedAction(action_name);
    base::UmaHistogramBoolean("InProductHelp.TextBubble.ShowSnooze",
                              result.should_show_snooze);
  } else {
    LogTriggerHelpUIResult(name, TriggerHelpUIResult::FAILURE);
    std::string action_name =
        "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.";
    action_name.append(feature.name);
    base::RecordComputedAction(action_name);
  }

  // Histogram about the failure reasons.
  if (!result.event_model_ready_ok) {
    LogTriggerHelpUIResult(name,
                           TriggerHelpUIResult::FAILURE_EVENT_MODEL_NOT_READY);
  }
  if (!result.currently_showing_ok) {
    LogTriggerHelpUIResult(name,
                           TriggerHelpUIResult::FAILURE_CURRENTLY_SHOWING);
  }
  if (!result.feature_enabled_ok) {
    LogTriggerHelpUIResult(name, TriggerHelpUIResult::FAILURE_FEATURE_DISABLED);
  }
  if (!result.config_ok) {
    LogTriggerHelpUIResult(name, TriggerHelpUIResult::FAILURE_CONFIG_INVALID);
  }
  if (!result.used_ok) {
    LogTriggerHelpUIResult(
        name, TriggerHelpUIResult::FAILURE_USED_PRECONDITION_UNMET);
  }
  if (!result.trigger_ok) {
    LogTriggerHelpUIResult(
        name, TriggerHelpUIResult::FAILURE_TRIGGER_PRECONDITION_UNMET);
  }
  if (!result.preconditions_ok) {
    LogTriggerHelpUIResult(
        name, TriggerHelpUIResult::FAILURE_OTHER_PRECONDITION_UNMET);
  }
  if (!result.session_rate_ok) {
    LogTriggerHelpUIResult(name, TriggerHelpUIResult::FAILURE_SESSION_RATE);
  }
  if (!result.availability_model_ready_ok) {
    LogTriggerHelpUIResult(
        name, TriggerHelpUIResult::FAILURE_AVAILABILITY_MODEL_NOT_READY);
  }
  if (!result.availability_ok) {
    LogTriggerHelpUIResult(
        name, TriggerHelpUIResult::FAILURE_AVAILABILITY_PRECONDITION_UNMET);
  }
  if (!result.display_lock_ok) {
    LogTriggerHelpUIResult(name, TriggerHelpUIResult::FAILURE_DISPLAY_LOCK);
  }
  if (!result.groups_ok) {
    LogTriggerHelpUIResult(
        name, TriggerHelpUIResult::FAILURE_GROUPS_PRECONDITION_UNMET);
  }
}

void RecordUserDismiss() {
  base::RecordAction(base::UserMetricsAction("InProductHelp.Dismissed"));
}

void RecordUserSnoozeAction(Tracker::SnoozeAction snooze_action) {
  base::UmaHistogramEnumeration("InProductHelp.SnoozeAction", snooze_action);
}

void RecordDbUpdate(bool success, StoreType type) {
  std::string histogram_name =
      "InProductHelp.Db.Update." + ToDbHistogramSuffix(type);
  base::UmaHistogramBoolean(histogram_name, success);
}

void RecordDbInitEvent(bool success, StoreType type) {
  std::string histogram_name =
      "InProductHelp.Db.Init." + ToDbHistogramSuffix(type);
  base::UmaHistogramBoolean(histogram_name, success);
}

void RecordEventDbLoadEvent(bool success, const std::vector<Event>& events) {
  std::string histogram_name =
      "InProductHelp.Db.Load." + ToDbHistogramSuffix(StoreType::EVENTS_STORE);
  base::UmaHistogramBoolean(histogram_name, success);

  if (!success)
    return;

  // Tracks total number of events records when the database is successfully
  // loaded.
  int event_count = 0;
  for (const auto& event : events)
    event_count += event.events_size();
  UMA_HISTOGRAM_COUNTS_1000("InProductHelp.Db.TotalEvents", event_count);
}

void RecordAvailabilityDbLoadEvent(bool success) {
  std::string histogram_name =
      "InProductHelp.Db.Load." +
      ToDbHistogramSuffix(StoreType::AVAILABILITY_STORE);
  base::UmaHistogramBoolean(histogram_name, success);
}

}  // namespace stats
}  // namespace feature_engagement
