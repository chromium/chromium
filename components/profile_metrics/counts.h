// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_METRICS_COUNTS_H_
#define COMPONENTS_PROFILE_METRICS_COUNTS_H_

#include "base/metrics/histogram_base.h"

namespace profile_metrics {

struct Counts {
  base::HistogramBase::Sample total;
  base::HistogramBase::Sample signedin;
  base::HistogramBase::Sample supervised;
  base::HistogramBase::Sample active;
  base::HistogramBase::Sample named;
  base::HistogramBase::Sample unused;
  base::HistogramBase::Sample gaia_icon;
  base::HistogramBase::Sample auth_errors;

  Counts()
      : total(0),
        signedin(0),
        supervised(0),
        active(0),
        named(0),
        unused(0),
        gaia_icon(0),
        auth_errors(0) {}
};

// Logs metrics related to |counts|.
void LogProfileMetricsCounts(const Counts& counts);

}  // namespace profile_metrics

#endif  // COMPONENTS_PROFILE_METRICS_COUNTS_H_
