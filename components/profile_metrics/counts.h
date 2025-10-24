// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_METRICS_COUNTS_H_
#define COMPONENTS_PROFILE_METRICS_COUNTS_H_

#include "base/metrics/histogram_base.h"

namespace profile_metrics {

struct Counts {
  base::HistogramBase::Sample32 total = 0;
  base::HistogramBase::Sample32 signedin = 0;
  base::HistogramBase::Sample32 supervised = 0;
  base::HistogramBase::Sample32 active = 0;
  base::HistogramBase::Sample32 unused = 0;
};

enum class ProfileActivityThreshold {
  kDuration1Day,
  kDuration7Days,
  kDuration28Days,  // Used as the default threshold check.
};

void LogTotalNumberOfProfiles(base::HistogramBase::Sample32 number_of_profiles);
// Logs metrics related to `counts`. If `counts.total` is 0, nothing is
// recorded.
void LogProfileMetricsCounts(const Counts& counts,
                             ProfileActivityThreshold activity_threshold);

}  // namespace profile_metrics

#endif  // COMPONENTS_PROFILE_METRICS_COUNTS_H_
