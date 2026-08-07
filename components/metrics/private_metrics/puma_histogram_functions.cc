// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/puma_histogram_functions.h"

#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "components/metrics/private_metrics/lom_recorder.h"
#include "components/metrics/private_metrics/private_metrics_features.h"

namespace metrics::private_metrics {

void PumaHistogramBoolean(PumaType puma_type,
                          std::string_view name,
                          bool sample,
                          std::optional<uint64_t> profile_id) {
  if (base::FeatureList::IsEnabled(metrics::private_metrics::kLomFeature)) {
    metrics::private_metrics::LomRecorder::Get()->RecordBoolean(
        puma_type, name, sample, profile_id);
    return;
  }
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      name, PumaTypeToHistogramFlags(puma_type));
  histogram->Add(sample);
}

void PumaHistogramExactLinear(PumaType puma_type,
                              std::string_view name,
                              int sample,
                              int exclusive_max,
                              std::optional<uint64_t> profile_id) {
  if (base::FeatureList::IsEnabled(metrics::private_metrics::kLomFeature)) {
    metrics::private_metrics::LomRecorder::Get()->RecordExactLinear(
        puma_type, name, sample, exclusive_max, profile_id);
    return;
  }
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      PumaTypeToHistogramFlags(puma_type));
  histogram->Add(sample);
}

}  // namespace metrics::private_metrics
