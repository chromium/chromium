// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/puma_histogram_functions.h"

#include "base/metrics/histogram.h"

namespace metrics::private_metrics {

void PumaHistogramBoolean(PumaType puma_type,
                          std::string_view name,
                          bool sample) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      name, PumaTypeToHistogramFlags(puma_type));
  histogram->Add(sample);
}

void PumaHistogramExactLinear(PumaType puma_type,
                              std::string_view name,
                              int sample,
                              int exclusive_max) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      PumaTypeToHistogramFlags(puma_type));
  histogram->Add(sample);
}

}  // namespace metrics::private_metrics
