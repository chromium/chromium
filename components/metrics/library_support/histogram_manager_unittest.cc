// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/library_support/histogram_manager.h"

#include <stdint.h>

#include <string>

#include "base/metrics/histogram_macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(HistogramManager, HistogramBucketFields) {
  // Capture histograms at the start of the test to avoid later GetDeltas()
  // calls picking them up.
  std::vector<uint8_t> data_init;
  HistogramManager::GetInstance()->GetDeltas(&data_init);

  // kNoFlags filter should record all histograms.
  UMA_HISTOGRAM_ENUMERATION("UmaHistogramManager", 1, 2);

  std::vector<uint8_t> data;
  EXPECT_TRUE(HistogramManager::GetInstance()->GetDeltas(&data));
  EXPECT_FALSE(data.empty());
  ChromeUserMetricsExtension uma_proto;
  EXPECT_TRUE(uma_proto.ParseFromArray(reinterpret_cast<const char*>(&data[0]),
                                       data.size()));
  EXPECT_FALSE(data.empty());

  const HistogramEventProto& histogram_proto =
      uma_proto.histogram_event(uma_proto.histogram_event_size() - 1);
  ASSERT_EQ(1, histogram_proto.bucket_size());
  EXPECT_LE(0, histogram_proto.bucket(0).min());
  EXPECT_LE(2, histogram_proto.bucket(0).max());
  EXPECT_EQ(1, histogram_proto.bucket(0).count());

  UMA_HISTOGRAM_ENUMERATION("UmaHistogramManager2", 2, 3);
  std::vector<uint8_t> data2;
  EXPECT_TRUE(HistogramManager::GetInstance()->GetDeltas(&data2));
  EXPECT_FALSE(data2.empty());
  ChromeUserMetricsExtension uma_proto2;
  EXPECT_TRUE(uma_proto2.ParseFromArray(
      reinterpret_cast<const char*>(&data2[0]), data2.size()));
  EXPECT_FALSE(data2.empty());

  const HistogramEventProto& histogram_proto2 =
      uma_proto2.histogram_event(uma_proto2.histogram_event_size() - 1);
  ASSERT_EQ(1, histogram_proto2.bucket_size());
  EXPECT_LE(0, histogram_proto2.bucket(0).min());
  EXPECT_LE(3, histogram_proto2.bucket(0).max());
  EXPECT_EQ(1, histogram_proto2.bucket(0).count());
}

}  // namespace metrics
