// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"

class PrefService;

namespace metrics {

class MetricsService;

// This class limits and documents access to metrics service helper methods.
// These methods are protected so each user has to inherit own program-specific
// specialization and enable access there by declaring friends.
class MetricsServiceAccessor {
 protected:
  // Constructor declared as protected to enable inheritance. Descendants should
  // disallow instantiation.
  MetricsServiceAccessor() {}

  // Returns whether metrics reporting is enabled, using the value of the
  // kMetricsReportingEnabled pref in |pref_service| to determine whether user
  // has enabled reporting.
  static bool IsMetricsReportingEnabled(PrefService* pref_service);

  // Registers a field trial name and group with |metrics_service| (if not
  // null), to be used to annotate a UMA report with a particular configuration
  // state. Returns true on success.
  // See the comment on SyntheticTrialRegistry::RegisterSyntheticFieldTrial()
  // for details.
  static bool RegisterSyntheticFieldTrial(MetricsService* metrics_service,
                                          base::StringPiece trial_name,
                                          base::StringPiece group_name);

  // Same as RegisterSyntheticFieldTrial above, but takes in the trial and group
  // names as hashes rather than computing those hashes from the strings.
  static bool RegisterSyntheticFieldTrialWithNameAndGroupHash(
      MetricsService* metrics_service,
      uint32_t trial_name_hash,
      uint32_t group_name_hash);

  // IsMetricsReportingEnabled() in non-official builds unconditionally returns
  // false. This results in different behavior for tests running in official vs
  // non-official builds. To get consistent behavior call this with true, which
  // forces non-official builds to look at the prefs value official builds look
  // at.
  static void SetForceIsMetricsReportingEnabledPrefLookup(bool value);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceAccessor);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
