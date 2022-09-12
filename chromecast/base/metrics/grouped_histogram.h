// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_GROUPED_HISTOGRAM_H_
#define CHROMECAST_BASE_METRICS_GROUPED_HISTOGRAM_H_

#include <string>

namespace chromecast {
namespace metrics {

// Registers a predefined list of histograms to be collected per-app.  Must be
// called before any histograms of the same name are used or registration will
// fail.
void PreregisterAllGroupedHistograms();

// Sets the current app name to be used for subsequent grouped histogram
// samples (a new metric is generated with the app name as a suffix).
void TagAppStartForGroupedHistograms(const std::string& app_name);

} // namespace metrics
} // namespace chromecast

#endif // CHROMECAST_BASE_METRICS_GROUPED_HISTOGRAM_H_
