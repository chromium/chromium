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
// LINT.ThenChange(//tools/metrics/histograms/variants.xml:ProfileIndex)

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

void ProfileMetricsService::UmaHistogramBoolean(std::string_view name,
                                                bool sample) const {
  base::UmaHistogramBoolean(name, sample);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramBoolean(base::StrCat({name, histogram_suffix_}), sample);
  }
}

void ProfileMetricsService::UmaHistogramCounts100(std::string_view name,
                                                  int sample) const {
  base::UmaHistogramCounts100(name, sample);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramCounts100(base::StrCat({name, histogram_suffix_}),
                                sample);
  }
}

void ProfileMetricsService::UmaHistogramCounts1000(std::string_view name,
                                                   int sample) const {
  base::UmaHistogramCounts1000(name, sample);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramCounts1000(base::StrCat({name, histogram_suffix_}),
                                 sample);
  }
}

void ProfileMetricsService::UmaHistogramCounts10000(std::string_view name,
                                                    int sample) const {
  base::UmaHistogramCounts10000(name, sample);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramCounts10000(base::StrCat({name, histogram_suffix_}),
                                  sample);
  }
}

void ProfileMetricsService::UmaHistogramCounts100000(std::string_view name,
                                                     int sample) const {
  base::UmaHistogramCounts100000(name, sample);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramCounts100000(base::StrCat({name, histogram_suffix_}),
                                   sample);
  }
}

void ProfileMetricsService::UmaHistogramSparse(std::string_view name,
                                               int sample) const {
  base::UmaHistogramSparse(name, sample);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramSparse(base::StrCat({name, histogram_suffix_}), sample);
  }
}

void ProfileMetricsService::UmaHistogramCustomTimes(std::string_view name,
                                                    base::TimeDelta sample,
                                                    base::TimeDelta min,
                                                    base::TimeDelta max,
                                                    size_t buckets) const {
  base::UmaHistogramCustomTimes(name, sample, min, max, buckets);
  if (!histogram_suffix_.empty()) {
    base::UmaHistogramCustomTimes(base::StrCat({name, histogram_suffix_}),
                                  sample, min, max, buckets);
  }
}

}  // namespace metrics
