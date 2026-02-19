// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profile_metrics_service.h"

#include "base/strings/stringprintf.h"

namespace {

// LINT.IfChange(histogram_suffix)
constexpr std::string_view kHistogramSuffixFormat = ".Profile%d";
constexpr size_t kMaxProfileIndexToLog = 19;
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/histograms.xml:ProfileIndex)

std::string GetHistogramSuffix(std::optional<size_t> profile_index) {
  if (!profile_index.has_value()) {
    return "";
  }
  if (profile_index > kMaxProfileIndexToLog) {
    return "";
  }
  return base::StringPrintf(kHistogramSuffixFormat, profile_index.value());
}

}  // namespace

namespace metrics {

ProfileMetricsService::ProfileMetricsService(ProfileMetricsContext context)
    : profile_metrics_context_(context),
      histogram_suffix_(GetHistogramSuffix(profile_metrics_context_)) {}

}  // namespace metrics
