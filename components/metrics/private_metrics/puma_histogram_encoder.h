// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PUMA_HISTOGRAM_ENCODER_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PUMA_HISTOGRAM_ENCODER_H_

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/puma_histogram_functions.h"
#include "third_party/metrics_proto/private_metrics/private_user_metrics.pb.h"

namespace metrics::private_metrics {

// PumaHistogramEncoder is responsible for encoding histograms into PUMA protos,
// which then can be used to upload PUMA records.
class PumaHistogramEncoder : public base::HistogramFlattener {
 public:
  // Creates a new encoder which will encode histograms into the given proto.
  explicit PumaHistogramEncoder(
      ::private_metrics::PrivateUserMetrics& puma_proto);

  PumaHistogramEncoder(const PumaHistogramEncoder&) = delete;
  PumaHistogramEncoder& operator=(const PumaHistogramEncoder&) = delete;

  ~PumaHistogramEncoder() override;

  // Encodes histogram deltas (i.e. data logged since the last call) into the
  // given PUMA proto. Only histograms with `required_flags` are included.
  static void EncodeHistogramDeltas(
      base::PumaType puma_type,
      ::private_metrics::PrivateUserMetrics& puma_proto);

 private:
  // base::HistogramFlattener:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override;

  // Target proto object where the histograms are being encoded.
  raw_ptr<::private_metrics::PrivateUserMetrics> puma_proto_;
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PUMA_HISTOGRAM_ENCODER_H_
