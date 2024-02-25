// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_STATS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_STATS_H_

namespace feature_engagement::stats {

// Used in the metrics to track the configuration parsing event.
// The failure reasons are not mutually exclusive.
// Out-dated entries shouldn't be deleted but marked as obsolete.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep this synced with the enum in //tools/metrics/histograms/enums.xml.
enum class ConfigParsingEvent {
  // The configuration is parsed correctly.
  SUCCESS = 0,

  // The configuration is invalid after parsing.
  FAILURE = 1,

  // Fails to parse the feature config because no field trial is found,
  // and there was no checked in configuration.
  FAILURE_NO_FIELD_TRIAL = 2,

  // Fails to parse the used event.
  FAILURE_USED_EVENT_PARSE = 3,

  // Used event is missing.
  FAILURE_USED_EVENT_MISSING = 4,

  // Fails to parse the trigger event.
  FAILURE_TRIGGER_EVENT_PARSE = 5,

  // Trigger event is missing.
  FAILURE_TRIGGER_EVENT_MISSING = 6,

  // Fails to parse other events.
  FAILURE_OTHER_EVENT_PARSE = 7,

  // Fails to parse the session rate comparator.
  FAILURE_SESSION_RATE_PARSE = 8,

  // Fails to parse the availability comparator.
  FAILURE_AVAILABILITY_PARSE = 9,

  // UnKnown key in configuration parameters.
  FAILURE_UNKNOWN_KEY = 10,

  // Fails to parse the session rate impact.
  FAILURE_SESSION_RATE_IMPACT_PARSE = 11,

  // Fails to parse the session rate impact.
  FAILURE_SESSION_RATE_IMPACT_UNKNOWN_FEATURE = 12,

  // Fails to parse the tracking only flag.
  FAILURE_TRACKING_ONLY_PARSE = 13,

  // Successfully read checked in configuration.
  SUCCESS_FROM_SOURCE = 14,

  // Fails to parse the snooze parameters.
  FAILURE_SNOOZE_PARAMS_PARSE = 15,

  // Fails to parse the blocked_by parameters.
  FAILURE_BLOCKED_BY_PARSE = 16,

  // Fails to parse the blocked_by feature list.
  FAILURE_BLOCKED_BY_UNKNOWN_FEATURE = 17,

  // Fails to parse the blocking parameters.
  FAILURE_BLOCKING_PARSE = 18,

  // Fails to pare the groups parameter.
  FAILURE_GROUPS_PARSE = 19,

  // Fails to parse the groups parameter groups list.
  FAILURE_GROUPS_UNKNOWN_GROUP = 20,

  // Last entry for the enum.
  COUNT = 21,
};

// Records configuration parsing event.
void RecordConfigParsingEvent(ConfigParsingEvent event);

}  // namespace feature_engagement::stats

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_STATS_H_
