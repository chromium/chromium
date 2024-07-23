// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace data_controls {

std::string GetDlpHistogramPrefix() {
  return "Enterprise.Dlp.";
}

void DlpBooleanHistogram(const std::string& suffix, bool value) {
  base::UmaHistogramBoolean(GetDlpHistogramPrefix() + suffix, value);
}

void DlpCountHistogram(const std::string& suffix, int sample, int max) {
  base::UmaHistogramExactLinear(GetDlpHistogramPrefix() + suffix, sample,
                                max + 1);
}

void DlpRestrictionConfiguredHistogram(Rule::Restriction value) {
  base::UmaHistogramEnumeration(
      GetDlpHistogramPrefix() + "RestrictionConfigured", value);
}

void DlpCountHistogram10000(const std::string& suffix, int sample) {
  base::UmaHistogramCounts10000(GetDlpHistogramPrefix() + suffix, sample);
}

}  // namespace data_controls
