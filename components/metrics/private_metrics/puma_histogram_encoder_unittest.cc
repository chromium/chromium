// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/puma_histogram_encoder.h"

#include "base/metrics/puma_histogram_functions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::private_metrics {

namespace {

using ::metrics::HistogramEventProto;
using ::private_metrics::PrivateUserMetrics;

}  // namespace

TEST(PumaHistogramEncoderTest, EncodesEmptyHistogramDeltas) {
  PrivateUserMetrics puma_proto;

  PumaHistogramEncoder::EncodeHistogramDeltas(base::PumaType::kRc, puma_proto);

  EXPECT_EQ(puma_proto.histogram_events_size(), 0);
}

TEST(PumaHistogramEncoderTest, EncodesHistogramDeltas) {
  base::PumaHistogramExactLinear(
      base::PumaType::kRc, "PumaHistogramEncoderTest.TestHistogram", 12, 100);

  PrivateUserMetrics puma_proto;

  PumaHistogramEncoder::EncodeHistogramDeltas(base::PumaType::kRc, puma_proto);

  EXPECT_EQ(puma_proto.histogram_events_size(), 1);

  HistogramEventProto histogram_proto = puma_proto.histogram_events()[0];

  EXPECT_TRUE(histogram_proto.has_name_hash());
  EXPECT_EQ(histogram_proto.bucket_size(), 1);
  EXPECT_EQ(histogram_proto.bucket()[0].count(), 1);
}

TEST(PumaHistogramEncoderTest, DoesNotDoubleEncodeHistogramDeltas) {
  base::PumaHistogramBoolean(base::PumaType::kRc,
                             "PumaHistogramEncoderTest.TestHistogram1", true);

  PrivateUserMetrics puma_proto1;
  PumaHistogramEncoder::EncodeHistogramDeltas(base::PumaType::kRc, puma_proto1);
  EXPECT_EQ(puma_proto1.histogram_events_size(), 1);

  PrivateUserMetrics puma_proto2;
  PumaHistogramEncoder::EncodeHistogramDeltas(base::PumaType::kRc, puma_proto2);
  EXPECT_EQ(puma_proto2.histogram_events_size(), 0);

  base::PumaHistogramBoolean(base::PumaType::kRc,
                             "PumaHistogramEncoderTest.TestHistogram2", true);

  PrivateUserMetrics puma_proto3;
  PumaHistogramEncoder::EncodeHistogramDeltas(base::PumaType::kRc, puma_proto3);
  EXPECT_EQ(puma_proto3.histogram_events_size(), 1);
}

}  // namespace metrics::private_metrics
