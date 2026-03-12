// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profile_metrics_service.h"

#include "base/strings/stringprintf.h"

namespace {

// Sharing variants definition is not currently supported across multiple
// sub-directories, so a duplication is necessary. List below (in
// LINT.ThenChange) all required histogram sub-directories that implement
// per profile metrics - the variants names must be unique: use
// "ProfileIndex{sub_dir_name}" for consistency.
//
// LINT.IfChange(histogram_suffix)
constexpr std::string_view kHistogramSuffixFormat = ".Profile%d";
constexpr size_t kMaxProfileIndexForIndividualLog = 19;
constexpr std::string_view kHistogramSuffixMaxProfileCountCombinedName =
    ".Profile%dPlus";
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:ProfileIndexSignin,
// //tools/metrics/histograms/metadata/profile/histograms.xml:ProfileIndexProfile)

std::string GetHistogramSuffix(std::optional<size_t> profile_index) {
  if (!profile_index.has_value()) {
    return "";
  }
  // For all out of scope profiles, combine the results into a single histogram.
  if (profile_index > kMaxProfileIndexForIndividualLog) {
    return base::StringPrintf(kHistogramSuffixMaxProfileCountCombinedName,
                              kMaxProfileIndexForIndividualLog + 1);
  }
  return base::StringPrintf(kHistogramSuffixFormat, profile_index.value());
}

}  // namespace

namespace metrics {

ProfileMetricsService::ProfileMetricsService(ProfileMetricsContext context)
    : profile_metrics_context_(context),
      histogram_suffix_(GetHistogramSuffix(profile_metrics_context_)) {}

}  // namespace metrics
