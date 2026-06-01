// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/histogram_encoder.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"

using base::SampleCountIterator;

namespace metrics {
namespace {

// A comma-separated list of histogram names to be skipped/excluded during
// serialization of metrics logs. When the HistogramDenylist feature is
// disabled, the default value specified here is used.
const base::FeatureParam<std::string> kHistogramDenylistParam{
    &kHistogramDenylist, "histogram_denylist",
    // TODO: crbug.com/518852743 - These histograms are currently problematic
    // as they bloat UMA logs, causing them to be trimmed. Remove once fixed.
    "Variations.FeatureAccess,Variations.FeatureAccessEarly"};

const std::vector<uint64_t>& GetHistogramDenylist() {
  static const base::NoDestructor<std::vector<uint64_t>> histogram_denylist([] {
    std::vector<uint64_t> hashes;
    for (std::string_view name : base::SplitStringPiece(
             kHistogramDenylistParam.Get(), ",", base::TRIM_WHITESPACE,
             base::SPLIT_WANT_NONEMPTY)) {
      hashes.push_back(base::HashMetricName(name));
    }
    return hashes;
  }());
  return *histogram_denylist;
}

}  // namespace

BASE_FEATURE(kHistogramDenylist, base::FEATURE_DISABLED_BY_DEFAULT);

bool EncodeHistogramDelta(
    std::string_view histogram_name,
    const base::HistogramSamples& snapshot,
    base::FunctionRef<HistogramEventProto*()> add_histogram_event) {
  DCHECK_NE(0, snapshot.TotalCount());

  uint64_t name_hash = base::HashMetricName(histogram_name);
  // Note: When the feature is disabled, `kHistogramDenylistParam`'s default
  // value is used.
  if (std::ranges::contains(GetHistogramDenylist(), name_hash)) {
    return false;
  }

  HistogramEventProto* histogram_proto = add_histogram_event();
  DCHECK(histogram_proto);

  // We will ignore the MAX_INT/infinite value in the last element of range[].

  histogram_proto->set_name_hash(name_hash);
  if (snapshot.sum() != 0) {
    histogram_proto->set_sum(snapshot.sum());
  }

  for (std::unique_ptr<SampleCountIterator> it = snapshot.Iterator();
       !it->Done(); it->Next()) {
    base::Histogram::Sample32 min;
    int64_t max;
    base::Histogram::Count32 count;
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
  return true;
}

}  // namespace metrics
