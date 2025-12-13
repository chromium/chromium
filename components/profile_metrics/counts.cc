// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_metrics/counts.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace profile_metrics {

namespace {

std::string GetHistogramSuffix(
    profile_metrics::ProfileActivityThreshold activity_threshold) {
  switch (activity_threshold) {
      // LINT.IfChange(ProfileActivityThreshold)
    case profile_metrics::ProfileActivityThreshold::kDuration1Day:
      return ".1Day";
    case profile_metrics::ProfileActivityThreshold::kDuration7Days:
      return ".7Days";
    case profile_metrics::ProfileActivityThreshold::kDuration28Days:
      return "";
      // LINT.ThenChange(//tools/metrics/histograms/metadata/profile/histograms.xml:ProfileActivityThreshold)
  }
}

}  // namespace

void LogTotalNumberOfProfiles(
    base::HistogramBase::Sample32 number_of_profiles) {
  base::UmaHistogramCounts100("Profile.NumberOfProfiles", number_of_profiles);
}

void LogProfileMetricsCounts(const Counts& counts,
                             ProfileActivityThreshold activity_threshold) {
  // Do not record specific metrics if we have no profiles.
  if (counts.total <= 0) {
    return;
  }

  std::string histogram_suffix = GetHistogramSuffix(activity_threshold);
  base::UmaHistogramCounts100(
      base::StrCat({"Profile.NumberOfManagedProfiles", histogram_suffix}),
      counts.supervised);
  base::UmaHistogramCounts100(
      base::StrCat({"Profile.PercentageOfManagedProfiles", histogram_suffix}),
      100 * counts.supervised / counts.total);
  base::UmaHistogramCounts100(
      base::StrCat({"Profile.NumberOfSignedInProfiles", histogram_suffix}),
      counts.signedin);
  base::UmaHistogramCounts100(
      base::StrCat({"Profile.NumberOfActiveProfiles", histogram_suffix}),
      counts.active);
  base::UmaHistogramCounts100(
      base::StrCat({"Profile.NumberOfUnusedProfiles", histogram_suffix}),
      counts.unused);
}

}  // namespace profile_metrics
