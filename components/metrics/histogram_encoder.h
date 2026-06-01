// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines an utility function that records any changes in a given
// histogram for transmission.

#ifndef COMPONENTS_METRICS_HISTOGRAM_ENCODER_H_
#define COMPONENTS_METRICS_HISTOGRAM_ENCODER_H_

#include <string_view>

#include "base/feature.h"
#include "base/functional/function_ref.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace base {
class HistogramSamples;
}

namespace metrics {

// Allows specifying a list of histogram names (separated with a comma) to be
// skipped/excluded during serialization of metrics logs (UMA and PUMA). Note
// that the histograms will still be recorded locally -- they just won't be put
// into logs (and therefore won't be uploaded).
BASE_DECLARE_FEATURE(kHistogramDenylist);

// Record any changes (histogram deltas of counts from |snapshot|) into
// |histogram_proto| for the given histogram (|histogram_name|). Returns false
// if the histogram changes are not recorded (i.e. due to histogram denylist).
bool EncodeHistogramDelta(
    std::string_view histogram_name,
    const base::HistogramSamples& snapshot,
    base::FunctionRef<HistogramEventProto*()> add_histogram_event);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_HISTOGRAM_ENCODER_H_
