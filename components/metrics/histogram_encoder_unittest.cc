// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/histogram_encoder.h"

#include <string>

#include "base/metrics/bucket_ranges.h"
#include "base/metrics/sample_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(HistogramEncoder, HistogramBucketFields) {
  // Create buckets: 1-5, 5-7, 7-8, 8-9, 9-10, 10-11, 11-12.
  base::BucketRanges ranges(8);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 7);
  ranges.set_range(3, 8);
  ranges.set_range(4, 9);
  ranges.set_range(5, 10);
  ranges.set_range(6, 11);
  ranges.set_range(7, 12);

  base::SampleVector samples(1, &ranges);
  samples.Accumulate(3, 1);   // Bucket 1-5.
  samples.Accumulate(6, 1);   // Bucket 5-7.
  samples.Accumulate(8, 1);   // Bucket 8-9. (7-8 skipped)
  samples.Accumulate(10, 1);  // Bucket 10-11. (9-10 skipped)
  samples.Accumulate(11, 1);  // Bucket 11-12.

  ChromeUserMetricsExtension uma_proto;
  EncodeHistogramDelta("Test", samples, &uma_proto);

  const HistogramEventProto& histogram_proto =
      uma_proto.histogram_event(uma_proto.histogram_event_size() - 1);

  // Buckets with samples: 1-5, 5-7, 8-9, 10-11, 11-12.
  // Should become: 1-/, 5-7, /-9, 10-/, /-12.
  ASSERT_EQ(5, histogram_proto.bucket_size());

  // 1-5 becomes 1-/ (max is same as next min).
  EXPECT_TRUE(histogram_proto.bucket(0).has_min());
  EXPECT_FALSE(histogram_proto.bucket(0).has_max());
  EXPECT_EQ(1, histogram_proto.bucket(0).min());

  // 5-7 stays 5-7 (no optimization possible).
  EXPECT_TRUE(histogram_proto.bucket(1).has_min());
  EXPECT_TRUE(histogram_proto.bucket(1).has_max());
  EXPECT_EQ(5, histogram_proto.bucket(1).min());
  EXPECT_EQ(7, histogram_proto.bucket(1).max());

  // 8-9 becomes /-9 (min is same as max - 1).
  EXPECT_FALSE(histogram_proto.bucket(2).has_min());
  EXPECT_TRUE(histogram_proto.bucket(2).has_max());
  EXPECT_EQ(9, histogram_proto.bucket(2).max());

  // 10-11 becomes 10-/ (both optimizations apply, omit max is prioritized).
  EXPECT_TRUE(histogram_proto.bucket(3).has_min());
  EXPECT_FALSE(histogram_proto.bucket(3).has_max());
  EXPECT_EQ(10, histogram_proto.bucket(3).min());

  // 11-12 becomes /-12 (last record must keep max, min is same as max - 1).
  EXPECT_FALSE(histogram_proto.bucket(4).has_min());
  EXPECT_TRUE(histogram_proto.bucket(4).has_max());
  EXPECT_EQ(12, histogram_proto.bucket(4).max());
}

}  // namespace metrics
