// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/puma_histogram_encoder.h"

#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/puma_histogram_functions.h"
#include "base/metrics/statistics_recorder.h"
#include "components/metrics/histogram_encoder.h"

namespace metrics::private_metrics {

using ::private_metrics::PrivateUserMetrics;

PumaHistogramEncoder::PumaHistogramEncoder(PrivateUserMetrics& puma_proto)
    : puma_proto_(&puma_proto) {}

PumaHistogramEncoder::~PumaHistogramEncoder() = default;

void PumaHistogramEncoder::RecordDelta(const base::HistogramBase& histogram,
                                       const base::HistogramSamples& snapshot) {
  EncodeHistogramDelta(histogram.histogram_name(), snapshot,
                       puma_proto_->add_histogram_events());
}

// static
void PumaHistogramEncoder::EncodeHistogramDeltas(
    base::PumaType puma_type,
    PrivateUserMetrics& puma_proto) {
  PumaHistogramEncoder encoder(puma_proto);
  base::HistogramSnapshotManager snapshot_manager(&encoder);

  base::StatisticsRecorder::PrepareDeltas(
      /*include_persistent=*/true,
      /*flags_to_set=*/base::Histogram::kNoFlags,
      /*required_flags=*/PumaTypeToHistogramFlags(puma_type),
      &snapshot_manager);
}

}  // namespace metrics::private_metrics
