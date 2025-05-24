// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>

#include <string_view>

#include "components/variations/synthetic_trials.h"

class PrefService;

namespace metrics {

class MetricsService;

// This class limits and documents access to metrics service helper methods.
// These methods are protected so each user has to inherit own program-specific
// specialization and enable access there by declaring friends.
class MetricsServiceAccessor {
 public:
  MetricsServiceAccessor(const MetricsServiceAccessor&) = delete;
  MetricsServiceAccessor& operator=(const MetricsServiceAccessor&) = delete;

  // Returns the value assigned by
  // SetForceIsMetricsReportingEnabledPrefLookup(). Default value is false.
  static bool IsForceMetricsReportingEnabledPrefLookup();

 protected:
  // Constructor declared as protected to enable inheritance. Descendants should
  // disallow instantiation.
  MetricsServiceAccessor() = default;

  // Returns whether metrics reporting is enabled, using the value of the
  // kMetricsReportingEnabled pref in |local_state| to determine whether user
  // has enabled reporting.
  static bool IsMetricsReportingEnabled(PrefService* local_state);

  // Registers a field trial name and group with |metrics_service| (if not
  // null), to be used to annotate a UMA report with a particular configuration
  // state. The |annotation_mode| parameter determines when UMA reports should
  // start being annotated with this trial and group. Returns true on success.
  // See the comment on SyntheticTrialRegistry::RegisterSyntheticFieldTrial()
  // and ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial() for more
  // details.
  static bool RegisterSyntheticFieldTrial(
      MetricsService* metrics_service,
      std::string_view trial_name,
      std::string_view group_name,
      variations::SyntheticTrialAnnotationMode annotation_mode);

  // IsMetricsReportingEnabled() in non-official builds unconditionally returns
  // false. This results in different behavior for tests running in official vs
  // non-official builds. To get consistent behavior call this with true, which
  // forces non-official builds to look at the prefs value official builds look
  // at.
  static void SetForceIsMetricsReportingEnabledPrefLookup(bool value);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
