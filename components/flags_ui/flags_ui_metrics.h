// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FLAGS_UI_METRICS_H_
#define COMPONENTS_FLAGS_UI_FLAGS_UI_METRICS_H_

#include <set>
#include <string>

#include "base/metrics/histogram_base.h"

namespace flags_ui {

// Returns the UMA id for the specified switch name.
base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name);

// Sends stats (as UMA histogram) about a set of command line |flags| in
// a histogram, with an enum value for each flag in |switches| and |features|,
// based on the hash of the flag name.
void ReportAboutFlagsHistogram(const std::string& uma_histogram_name,
                               const std::set<std::string>& switches,
                               const std::set<std::string>& features);

namespace testing {

// This value is reported as switch histogram ID if switch name has unknown
// format.
extern const base::HistogramBase::Sample kBadSwitchFormatHistogramId;

}  // namespace testing

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_FLAGS_UI_METRICS_H_
