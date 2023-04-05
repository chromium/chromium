// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/histogram_encoder.h"

#include <memory>
#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"

using base::SampleCountIterator;

namespace metrics {

void EncodeHistogramDelta(const std::string& histogram_name,
                          const base::HistogramSamples& snapshot,
                          ChromeUserMetricsExtension* uma_proto) {
  DCHECK_NE(0, snapshot.TotalCount());
  DCHECK(uma_proto);

  // We will ignore the MAX_INT/infinite value in the last element of range[].

  HistogramEventProto* histogram_proto = uma_proto->add_histogram_event();
  histogram_proto->set_name_hash(base::HashMetricName(histogram_name));
  if (snapshot.sum() != 0)
    histogram_proto->set_sum(snapshot.sum());

  for (std::unique_ptr<SampleCountIterator> it = snapshot.Iterator();
       !it->Done(); it->Next()) {
    base::Histogram::Sample min;
    int64_t max;
    base::Histogram::Count count;
    it->Get(&min, &max, &count);
    HistogramEventProto::Bucket* bucket = histogram_proto->add_bucket();
    bucket->set_min(min);
    bucket->set_max(max);
    // Note: The default for count is 1 in the proto, so omit it in that case.
    // The iterator also skips over empty buckets, so no need to manually omit
    // them.
    if (count != 1)
      bucket->set_count(count);
  }

  // Omit fields to save space (see rules in histogram_event.proto comments).
  for (int i = 0; i < histogram_proto->bucket_size(); ++i) {
    HistogramEventProto::Bucket* bucket = histogram_proto->mutable_bucket(i);
    if (i + 1 < histogram_proto->bucket_size() &&
        bucket->max() == histogram_proto->bucket(i + 1).min()) {
      bucket->clear_max();
    } else if (bucket->max() == bucket->min() + 1) {
      bucket->clear_min();
    }
  }
}

}  // namespace metrics
