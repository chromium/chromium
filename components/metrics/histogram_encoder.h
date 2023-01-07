// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines an utility function that records any changes in a given
// histogram for transmission.

#ifndef COMPONENTS_METRICS_HISTOGRAM_ENCODER_H_
#define COMPONENTS_METRICS_HISTOGRAM_ENCODER_H_

#include <string>

#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace base {
class HistogramSamples;
}

namespace metrics {

// Record any changes (histogram deltas of counts from |snapshot|) into
// |uma_proto| for the given histogram (|histogram_name|).
void EncodeHistogramDelta(const std::string& histogram_name,
                          const base::HistogramSamples& snapshot,
                          ChromeUserMetricsExtension* uma_proto);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_HISTOGRAM_ENCODER_H_
